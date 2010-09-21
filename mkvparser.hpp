// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef MKVPARSER_HPP
#define MKVPARSER_HPP

#include <cstdlib>
#include <cstdio>

namespace mkvparser
{

const int E_FILE_FORMAT_INVALID = -2;
const int E_BUFFER_NOT_FULL = -3;

class IMkvReader
{
public:
    virtual int Read(long long pos, long len, unsigned char* buf) = 0;
    virtual int Length(long long* total, long long* available) = 0;
protected:
    virtual ~IMkvReader();
};

long long GetUIntLength(IMkvReader*, long long, long&);
long long ReadUInt(IMkvReader*, long long, long&);
long long SyncReadUInt(IMkvReader*, long long pos, long long stop, long&);
long long UnserializeUInt(IMkvReader*, long long pos, long long size);
float Unserialize4Float(IMkvReader*, long long);
double Unserialize8Double(IMkvReader*, long long);
short Unserialize2SInt(IMkvReader*, long long);
signed char Unserialize1SInt(IMkvReader*, long long);
bool Match(IMkvReader*, long long&, unsigned long, long long&);
bool Match(IMkvReader*, long long&, unsigned long, char*&);
bool Match(IMkvReader*, long long&, unsigned long,unsigned char*&, size_t&);
bool Match(IMkvReader*, long long&, unsigned long, double&);
bool Match(IMkvReader*, long long&, unsigned long, short&);

void GetVersion(int& major, int& minor, int& build, int& revision);

struct EBMLHeader
{
    EBMLHeader();
    ~EBMLHeader();
    long long m_version;
    long long m_readVersion;
    long long m_maxIdLength;
    long long m_maxSizeLength;
    char* m_docType;
    long long m_docTypeVersion;
    long long m_docTypeReadVersion;

    long long Parse(IMkvReader*, long long&);
};


class Segment;
class Track;
class Cluster;

class Block
{
    Block(const Block&);
    Block& operator=(const Block&);

public:
    const long long m_start;
    const long long m_size;

    Block(long long start, long long size, IMkvReader*);

    unsigned long GetTrackNumber() const;

    long long GetTimeCode(Cluster*) const;  //absolute, but not scaled
    long long GetTime(Cluster*) const;      //absolute, and scaled (ns units)
    bool IsKey() const;
    void SetKey(bool);

    long GetSize() const;
    long Read(IMkvReader*, unsigned char*) const;

private:
    long long m_track;   //Track::Number()
    short m_timecode;  //relative to cluster
    unsigned char m_flags;
    long long m_frameOff;
    long m_frameSize;

};


class BlockEntry
{
    BlockEntry(const BlockEntry&);
    BlockEntry& operator=(const BlockEntry&);

public:
    virtual ~BlockEntry();
    virtual bool EOS() const = 0;
    virtual Cluster* GetCluster() const = 0;
    virtual size_t GetIndex() const = 0;
    virtual const Block* GetBlock() const = 0;
    virtual bool IsBFrame() const = 0;

protected:
    BlockEntry();

};


class SimpleBlock : public BlockEntry
{
    SimpleBlock(const SimpleBlock&);
    SimpleBlock& operator=(const SimpleBlock&);

public:
    SimpleBlock(Cluster*, size_t, long long start, long long size);

    bool EOS() const;
    Cluster* GetCluster() const;
    size_t GetIndex() const;
    const Block* GetBlock() const;
    bool IsBFrame() const;

protected:
    Cluster* const m_pCluster;
    const size_t m_index;
    Block m_block;

};


class BlockGroup : public BlockEntry
{
    BlockGroup(const BlockGroup&);
    BlockGroup& operator=(const BlockGroup&);

public:
    BlockGroup(Cluster*, size_t, long long, long long);
    ~BlockGroup();

    bool EOS() const;
    Cluster* GetCluster() const;
    size_t GetIndex() const;
    const Block* GetBlock() const;
    bool IsBFrame() const;

    short GetPrevTimeCode() const;  //relative to block's time
    short GetNextTimeCode() const;  //as above

protected:
    Cluster* const m_pCluster;
    const size_t m_index;

private:
    BlockGroup(Cluster*, size_t, unsigned long);
    void ParseBlock(long long start, long long size);

    short m_prevTimeCode;
    short m_nextTimeCode;

    //TODO: the Matroska spec says you can have multiple blocks within the
    //same block group, with blocks ranked by priority (the flag bits).
    //For now we just cache a single block.
#if 0
    typedef std::deque<Block*> blocks_t;
    blocks_t m_blocks;  //In practice should contain only a single element.
#else
    Block* m_pBlock;
#endif

};


class Track
{
    Track(const Track&);
    Track& operator=(const Track&);

public:
    Segment* const m_pSegment;
    virtual ~Track();

    long long GetType() const;
    unsigned long GetNumber() const;
    const char* GetNameAsUTF8() const;
    const char* GetCodecNameAsUTF8() const;
    const char* GetCodecId() const;
    const unsigned char* GetCodecPrivate(size_t&) const;

    const BlockEntry* GetEOS() const;

    struct Settings
    {
        long long start;
        long long size;
    };

    struct Info
    {
        long long type;
        long long number;
        long long uid;
        char* nameAsUTF8;
        char* codecId;
        unsigned char* codecPrivate;
        size_t codecPrivateSize;
        char* codecNameAsUTF8;
        Settings settings;
        Info();
        void Clear();
    };

    long GetFirst(const BlockEntry*&) const;
    long GetNext(const BlockEntry* pCurr, const BlockEntry*& pNext) const;
    virtual bool VetEntry(const BlockEntry*) const = 0;

protected:
    Track(Segment*, const Info&);
    const Info m_info;

    class EOSBlock : public BlockEntry
    {
    public:
        EOSBlock();

        bool EOS() const;
        Cluster* GetCluster() const;
        size_t GetIndex() const;
        const Block* GetBlock() const;
        bool IsBFrame() const;
    };

    EOSBlock m_eos;

};


class VideoTrack : public Track
{
    VideoTrack(const VideoTrack&);
    VideoTrack& operator=(const VideoTrack&);

public:
    VideoTrack(Segment*, const Info&);
    long long GetWidth() const;
    long long GetHeight() const;
    double GetFrameRate() const;

    bool VetEntry(const BlockEntry*) const;

private:
    long long m_width;
    long long m_height;
    double m_rate;

};


class AudioTrack : public Track
{
    AudioTrack(const AudioTrack&);
    AudioTrack& operator=(const AudioTrack&);

public:
    AudioTrack(Segment*, const Info&);
    double GetSamplingRate() const;
    long long GetChannels() const;
    long long GetBitDepth() const;
    bool VetEntry(const BlockEntry*) const;

private:
    double m_rate;
    long long m_channels;
    long long m_bitDepth;
};


class Tracks
{
    Tracks(const Tracks&);
    Tracks& operator=(const Tracks&);

public:
    Segment* const m_pSegment;
    const long long m_start;
    const long long m_size;

    Tracks(Segment*, long long start, long long size);
    virtual ~Tracks();

    Track* GetTrackByNumber(unsigned long tn) const;
    Track* GetTrackByIndex(unsigned long idx) const;

private:
    Track** m_trackEntries;
    Track** m_trackEntriesEnd;

    void ParseTrackEntry(long long, long long, Track*&);

public:
    unsigned long GetTracksCount() const;
};


class SegmentInfo
{
    SegmentInfo(const SegmentInfo&);
    SegmentInfo& operator=(const SegmentInfo&);

public:
    Segment* const m_pSegment;
    const long long m_start;
    const long long m_size;

    SegmentInfo(Segment*, long long start, long long size);
    ~SegmentInfo();
    long long GetTimeCodeScale() const;
    long long GetDuration() const;  //scaled
    const char* GetMuxingAppAsUTF8() const;
    const char* GetWritingAppAsUTF8() const;
    const char* GetTitleAsUTF8() const;

private:
    long long m_timecodeScale;
    double m_duration;
    char* m_pMuxingAppAsUTF8;
    char* m_pWritingAppAsUTF8;
    char* m_pTitleAsUTF8;
};


class CuePoint
{
public:
    void Parse(IMkvReader*, long long start, long long size);

    long long m_timecode;               //absolute but unscaled
    long long GetTime(Segment*) const;  //absolute and scaled (ns units)

    struct TrackPosition
    {
        long long m_track;
        long long m_pos;  //of cluster
        long long m_block;
        //codec_state  //defaults to 0
        //reference = clusters containing req'd referenced blocks
        //  reftime = timecode of the referenced block
    };

#if 0  //TODO
    typedef std::list<TrackPosition> track_positions_t;
    track_positions_t m_track_positions;

    const TrackPosition* Find(const Track*) const;

    class CompareTime : std::binary_function<long long, CuePoint, bool>
    {
        CompareTime& operator=(const CompareTime&);
    public:
        Segment* const m_pSegment;

        explicit CompareTime(Segment* p) : m_pSegment(p) {}
        CompareTime(const CompareTime& rhs) : m_pSegment(rhs.m_pSegment) {}

        long long GetTime(const CuePoint& cp) const
        {
            return cp.GetTime(m_pSegment);
        }

        bool operator()(long long left_ns, const CuePoint& cp) const
        {
            return (left_ns < GetTime(cp));
        }

        bool operator()(const CuePoint& cp, long long right_ns) const
        {
            return (GetTime(cp) < right_ns);
        }

        bool operator()(const CuePoint& lhs, const CuePoint& rhs) const
        {
            return (lhs.m_timecode < rhs.m_timecode);
        }
    };
#endif

private:
    void ParseTrackPosition(IMkvReader*, long long, long long);

};


class Cues
{
    Cues(const Cues&);
    Cues& operator=(const Cues&);

public:
    Segment* const m_pSegment;
    const long long m_start;
    const long long m_size;

    Cues(Segment*, long long start, long long size);

    bool Find(  //lower bound of time_ns
        long long time_ns,
        const Track*,
        const CuePoint*&,
        const CuePoint::TrackPosition*&) const;

    bool FindNext(  //upper_bound of time_ns
        long long time_ns,
        const Track*,
        const CuePoint*&,
        const CuePoint::TrackPosition*&) const;

private:
    CuePoint* m_cue_points;
    size_t m_cue_counts_count;

};


class Cluster
{
    Cluster(const Cluster&);
    Cluster& operator=(const Cluster&);

public:
    Segment* const m_pSegment;
    const size_t m_index;

public:
    static Cluster* Parse(Segment*, size_t, long long off);

    Cluster();  //EndOfStream
    ~Cluster();

    bool EOS() const;

    long long GetTimeCode();   //absolute, but not scaled
    long long GetTime();       //absolute, and scaled (nanosecond units)
    long long GetFirstTime();  //time (ns) of first (earliest) block

    const BlockEntry* GetFirst();
    const BlockEntry* GetLast();
    const BlockEntry* GetNext(const BlockEntry*) const;
    const BlockEntry* GetEntry(const Track*);
    const BlockEntry* GetMaxKey(const VideoTrack*);

protected:
    Cluster(Segment*, size_t, long long off);

private:
    long long m_start;
    long long m_size;
    long long m_timecode;
    BlockEntry** m_pEntries;
    size_t m_entriesCount;

    void Load();
    void LoadBlockEntries();
    void ParseBlockGroup(long long, long long, size_t);
    void ParseSimpleBlock(long long, long long, size_t);

};


class Segment
{
    Segment(const Segment&);
    Segment& operator=(const Segment&);

private:
    Segment(IMkvReader*, long long pos, long long size);

public:
    IMkvReader* const m_pReader;
    const long long m_start;  //posn of segment payload
    const long long m_size;   //size of segment payload
    Cluster m_eos;  //TODO: make private?

    static long long CreateInstance(IMkvReader*, long long, Segment*&);
    ~Segment();

    //for big-bang loading (source filter)
    long Load();

    //for incremental loading (splitter)
    long long Unparsed() const;
    long long ParseHeaders();
    long ParseCluster(Cluster*&, long long& newpos) const;
    bool AddCluster(Cluster*, long long);

    Tracks* GetTracks() const;
    const SegmentInfo* const GetInfo() const;
    long long GetDuration() const;

    //NOTE: this turned out to be too inefficient.
    //long long Load(long long time_nanoseconds);

    Cluster* GetFirst();
    Cluster* GetLast();
    unsigned long GetCount() const;

    Cluster* GetNext(const Cluster*);
    Cluster* GetCluster(long long time_nanoseconds);

    void GetCluster(
        long long time_nanoseconds,
        Track*,
        Cluster*&,
        const BlockEntry*&);

    const Cues* GetCues() const;

private:
    long long m_pos;  //absolute file posn; what has been consumed so far
    SegmentInfo* m_pInfo;
    Tracks* m_pTracks;
    Cues* m_pCues;
    Cluster** m_clusters;
    size_t m_clusterCount;  //number of entries
    size_t m_clusterSize;   //array size

    void ParseSeekHead(long long pos, long long size, size_t*);
    void ParseSeekEntry(long long pos, long long size, size_t*);
    void ParseSecondarySeekHead(long long off, size_t*);
    void ParseCues(long long off);

    bool SearchCues(
        long long time_ns,
        Track*,
        Cluster*&,
        const BlockEntry*&);

};


}  //end namespace mkvparser

#endif  //MKVPARSER_HPP
