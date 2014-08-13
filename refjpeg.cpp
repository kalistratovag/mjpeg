#include "opencv2/core.hpp"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#if _MSC_VER >= 1200
    #pragma warning( disable: 4711 4324 )
#endif

namespace cv
{
namespace jpeg
{

#define  RBS_THROW_EOS    -123  /* <end of stream> exception code */
#define  RBS_THROW_FORB   -124  /* <forrbidden huffman code> exception code */
#define  RBS_HUFF_FORB    2047  /* forrbidden huffman code "value" */

typedef unsigned char uchar;

#define saturate(x) (uchar)((x) < 0 ? 0 : (x) > 255 ? 255 : (x))

//  Standard JPEG quantization tables
static const uchar jpegTableK1_T[] =
{
    16, 12, 14, 14,  18,  24,  49,  72,
    11, 12, 13, 17,  22,  35,  64,  92,
    10, 14, 16, 22,  37,  55,  78,  95,
    16, 19, 24, 29,  56,  64,  87,  98,
    24, 26, 40, 51,  68,  81, 103, 112,
    40, 58, 57, 87, 109, 104, 121, 100,
    51, 60, 69, 80, 103, 113, 120, 103,
    61, 55, 56, 62,  77,  92, 101,  99
};


static const uchar jpegTableK2_T[] =
{
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99
};


// Standard Huffman tables

// ... for luma DCs.
static const uchar jpegTableK3[] =
{
    0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
};


// ... for chroma DCs.
static const uchar jpegTableK4[] =
{
    0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
};


// ... for luma ACs.
static const uchar jpegTableK5[] =
{
    0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 125,
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
    0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
    0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
    0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
    0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
    0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
    0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
    0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
    0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
    0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
};

// ... for chroma ACs
static const uchar jpegTableK6[] =
{
    0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 119,
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
    0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
    0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
    0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
    0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
    0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
    0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
    0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
    0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
    0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
    0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
    0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
    0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
    0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
    0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
    0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
    0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
};


// zigzag & IDCT prescaling (AAN algorithm) tables
static const uchar zigzag[] =
{
    0,  8,  1,  2,  9, 16, 24, 17, 10,  3,  4, 11, 18, 25, 32, 40,
    33, 26, 19, 12,  5,  6, 13, 20, 27, 34, 41, 48, 56, 49, 42, 35,
    28, 21, 14,  7, 15, 22, 29, 36, 43, 50, 57, 58, 51, 44, 37, 30,
    23, 31, 38, 45, 52, 59, 60, 53, 46, 39, 47, 54, 61, 62, 55, 63,
    63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63
};


static const int idct_prescale[] =
{
    16384, 22725, 21407, 19266, 16384, 12873,  8867,  4520,
    22725, 31521, 29692, 26722, 22725, 17855, 12299,  6270,
    21407, 29692, 27969, 25172, 21407, 16819, 11585,  5906,
    19266, 26722, 25172, 22654, 19266, 15137, 10426,  5315,
    16384, 22725, 21407, 19266, 16384, 12873,  8867,  4520,
    12873, 17855, 16819, 15137, 12873, 10114,  6967,  3552,
    8867, 12299, 11585, 10426,  8867,  6967,  4799,  2446,
    4520,  6270,  5906,  5315,  4520,  3552,  2446,  1247
};


static const char jpegHeader[] =
"\xFF\xD8"  // SOI  - start of image
"\xFF\xE0"  // APP0 - jfif extention
"\x00\x10"  // 2 bytes: length of APP0 segment
"JFIF\x00"  // JFIF signature
"\x01\x02"  // version of JFIF
"\x00"      // units = pixels ( 1 - inch, 2 - cm )
"\x00\x01\x00\x01" // 2 2-bytes values: x density & y density
"\x00\x00"; // width & height of thumbnail: ( 0x0 means no thumbnail)

#define fixb         14
#define fix(x, n)    (int)((x)*(1 << (n)) + .5)
#define fix1(x, n)   (x)
#define fixmul(x)    (x)
#define  descale(x,n)  (((x) + (1 << ((n)-1))) >> (n))

#define C0_707     fix( 0.707106781f, fixb )
#define C0_924     fix( 0.923879533f, fixb )
#define C0_541     fix( 0.541196100f, fixb )
#define C0_382     fix( 0.382683432f, fixb )
#define C1_306     fix( 1.306562965f, fixb )

#define C1_082     fix( 1.082392200f, fixb )
#define C1_414     fix( 1.414213562f, fixb )
#define C1_847     fix( 1.847759065f, fixb )
#define C2_613     fix( 2.613125930f, fixb )

#define postshift 14

#define fixc       12
#define b_cb       fix( 1.772, fixc )
#define g_cb      -fix( 0.34414, fixc )
#define g_cr      -fix( 0.71414, fixc )
#define r_cr       fix( 1.402, fixc )
    
#define y_r        fix( 0.299, fixc )
#define y_g        fix( 0.587, fixc )
#define y_b        fix( 0.114, fixc )
    
#define cb_r      -fix( 0.1687, fixc )
#define cb_g      -fix( 0.3313, fixc )
#define cb_b       fix( 0.5,    fixc )
    
#define cr_r       fix( 0.5,    fixc )
#define cr_g      -fix( 0.4187, fixc )
#define cr_b      -fix( 0.0813, fixc )


// class RBaseStream - base class for other reading streams.
class RBaseStream
{
public:
    //methods
    RBaseStream();
    virtual ~RBaseStream();
    
    virtual bool  Open( const char* filename );
    virtual void  Close();
    void          SetBlockSize( int block_size, int unGetsize = 4 );
    bool          IsOpened();
    void          SetPos( int pos );
    int           GetPos();
    void          Skip( int bytes );
    jmp_buf&      JmpBuf();
    
protected:
    
    jmp_buf m_jmp_buf;
    uchar*  m_start;
    uchar*  m_end;
    uchar*  m_current;
    FILE*   m_file;
    int     m_unGetsize;
    int     m_block_size;
    int     m_block_pos;
    bool    m_jmp_set;
    bool    m_is_opened;

    virtual void  ReadBlock();
    virtual void  Release();
    virtual void  Allocate();
};


// class RLByteStream - uchar-oriented stream.
// l in prefix means that the least significant uchar of a multi-uchar value goes first
class RLByteStream : public RBaseStream
{
public:
    virtual ~RLByteStream();
    
    int     GetByte();
    void    GetBytes( void* buffer, int count, int* readed = 0 );
    int     GetWord();
    int     GetDWord(); 
};

// class RMBitStream - uchar-oriented stream.
// m in prefix means that the most significant uchar of a multi-uchar value go first
class RMByteStream : public RLByteStream
{
public:
    virtual ~RMByteStream();

    int     GetWord();
    int     GetDWord(); 
};

// class RLBitStream - bit-oriented stream.
// l in prefix means that the least significant bit of a multi-bit value goes first
class RLBitStream : public RBaseStream
{
public:
    virtual ~RLBitStream();
    
    void    SetPos( int pos );
    int     GetPos();
    int     Get( int bits );
    int     Show( int bits );
    int     GetHuff( const short* table );
    void    Move( int shift );
    void    Skip( int bytes );
        
protected:
    int     m_bit_idx;
    virtual void  ReadBlock();
};

// class RMBitStream - bit-oriented stream.
// m in prefix means that the most significant bit of a multi-bit value goes first
class RMBitStream : public RLBitStream
{
public:
    virtual ~RMBitStream();
    
    void    SetPos( int pos );
    int     GetPos();
    int     Get( int bits );
    int     Show( int bits );
    int     GetHuff( const short* table );
    void    Move( int shift );
    void    Skip( int bytes );

protected:
    virtual void  ReadBlock();
};


#define  BS_DEF_BLOCK_SIZE   (1<<10)

// WBaseStream - base class for output streams
class WBaseStream
{
public:
    //methods
    WBaseStream();
    virtual ~WBaseStream();
    
    virtual bool  Open( const char* filename );
    virtual void  Close();
    void          SetBlockSize( int block_size );
    bool          IsOpened();
    int           GetPos();
    virtual void  WriteBlock();
    
protected:

    uchar   m_buf[BS_DEF_BLOCK_SIZE+256];
    uchar*  m_start;
    uchar*  m_end;
    uchar*  m_current;
    int     m_block_size;
    int     m_block_pos;
    FILE*   m_file;
    bool    m_is_opened;

    virtual void  Release();
    virtual void  Allocate();
};


// class WLByteStream - uchar-oriented stream.
// l in prefix means that the least significant uchar of a multi-byte value goes first
class WLByteStream : public WBaseStream
{
public:
    virtual ~WLByteStream();

    void    PutByte( int val );
    void    PutBytes( const void* buffer, int count );
    void    PutWord( int val );
    void    PutDWord( int val ); 
};


// class WLByteStream - uchar-oriented stream.
// m in prefix means that the least significant uchar of a multi-byte value goes last
class WMByteStream : public WLByteStream
{
public:
    virtual ~WMByteStream();

    void    PutWord( int val );
    void    PutDWord( int val ); 
};


// class WLBitStream - bit-oriented stream.
// l in prefix means that the least significant bit of a multi-bit value goes first
class WLBitStream : public WBaseStream
{
public:
    virtual ~WLBitStream();
    
    int     GetPos();
    void    Put( int val, int bits );
    void    PutHuff( int val, const int* table );
    virtual void  WriteBlock();
        
protected:
    int     m_bit_idx;
    int     m_val;
};


// class WMBitStream - bit-oriented stream.
// l in prefix means that the least significant bit of a multi-bit value goes first
class WMBitStream : public WBaseStream
{
public:
    WMBitStream();
    virtual ~WMBitStream();
    
    bool    Open( const char* filename );
    void    Close();
    virtual void  Flush();

    int     GetPos();
    void    Put( int val, int bits );
    void    PutHuff( int val, const unsigned* table );
    virtual void  WriteBlock();
        
protected:
    int     m_bit_idx;
    unsigned   m_pad_val;
    unsigned   m_val;
    void    ResetBuffer();
};



#define BSWAP(v)    (((v)<<24)|(((v)&0xff00)<<8)| \
                    (((v)>>8)&0xff00)|((unsigned)(v)>>24))

int* bsCreateSourceHuffmanTable( const uchar* src, int* dst, 
                                 int max_bits, int first_bits );
bool bsCreateDecodeHuffmanTable( const int* src, short* dst, int max_size );
bool bsCreateEncodeHuffmanTable( const int* src, unsigned* dst, int max_size );

void bsBSwapBlock( uchar *start, uchar *end );
bool bsIsBigEndian( void );

extern const unsigned bs_bit_mask[];

const unsigned bs_bit_mask[] = {
    0,
    0x00000001, 0x00000003, 0x00000007, 0x0000000F,
    0x0000001F, 0x0000003F, 0x0000007F, 0x000000FF,
    0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF,
    0x00001FFF, 0x00003FFF, 0x00007FFF, 0x0000FFFF,
    0x0001FFFF, 0x0003FFFF, 0x0007FFFF, 0x000FFFFF,
    0x001FFFFF, 0x003FFFFF, 0x007FFFFF, 0x00FFFFFF,
    0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF,
    0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF
};

void bsBSwapBlock( uchar *start, uchar *end )
{
    unsigned* data = (unsigned*)start;
    int i, size = (int)(end - start+3)/4;

    for( i = 0; i < size; i++ )
    {
        unsigned temp = data[i];
        temp = BSWAP( temp );
        data[i] = temp;
    }
}

bool  bsIsBigEndian( void )
{
    return (((const int*)"\0\x1\x2\x3\x4\x5\x6\x7")[0] & 255) != 0;
}

/////////////////////////  RBaseStream ////////////////////////////

bool  RBaseStream::IsOpened()
{ 
    return m_is_opened;
}

void  RBaseStream::Allocate()
{
    if( !m_start )
    {
        m_start = new uchar[m_block_size + m_unGetsize];
        m_start+= m_unGetsize;
    }
    m_end = m_start + m_block_size;
    m_current = m_end;
}


RBaseStream::RBaseStream()
{
    m_start = m_end = m_current = 0;
    m_file = 0;
    m_block_size = BS_DEF_BLOCK_SIZE;
    m_unGetsize = 4; // 32 bits
    m_is_opened = false;
    m_jmp_set = false;
}


RBaseStream::~RBaseStream()
{
    Close();    // Close files
    Release();  // free  buffers
}


void  RBaseStream::ReadBlock()
{
    size_t readed;
    assert( m_file != 0 );

    // copy unget buffer
    if( m_start )
    {
        memcpy( m_start - m_unGetsize, m_end - m_unGetsize, m_unGetsize );
    }

    SetPos( GetPos() ); // normalize position

    fseek( m_file, m_block_pos, SEEK_SET );
    readed = fread( m_start, 1, m_block_size, m_file );
    m_end = m_start + readed;
    m_current   -= m_block_size;
    m_block_pos += m_block_size;

    if( readed == 0 || m_current >= m_end )
    {
        if( m_jmp_set )
            longjmp( m_jmp_buf, RBS_THROW_EOS );
    }
}


bool  RBaseStream::Open( const char* filename )
{
    Close();
    Allocate();
    
    m_file = fopen( filename, "rb" );
    
    if( m_file )
    {
        m_is_opened = true;
        SetPos(0);
    }
    return m_file != 0;
}

void  RBaseStream::Close()
{
    if( m_file )
    {
        fclose( m_file );
        m_file = 0;
    }
    m_is_opened = false;
}


void  RBaseStream::Release()
{
    if( m_start )
    {
        delete[] (m_start - m_unGetsize);
    }
    m_start = m_end = m_current = 0;
}


void  RBaseStream::SetBlockSize( int block_size, int unGetsize )
{
    assert( unGetsize >= 0 && block_size > 0 &&
           (block_size & (block_size-1)) == 0 );

    if( m_start && block_size == m_block_size && unGetsize == m_unGetsize ) return;
    Release();
    m_block_size = block_size;
    m_unGetsize = unGetsize;
    Allocate();
}


void  RBaseStream::SetPos( int pos )
{
    int offset = pos & (m_block_size - 1);
    int block_pos = pos - offset;
    
    assert( IsOpened() && pos >= 0 );
    
    if( m_current < m_end && block_pos == m_block_pos - m_block_size )
    {
        m_current = m_start + offset;
    }
    else
    {
        m_block_pos = block_pos;
        m_current = m_start + m_block_size + offset;
    }
}


int  RBaseStream::GetPos()
{
    assert( IsOpened() );
    return m_block_pos - m_block_size + (int)(m_current - m_start);
}

void  RBaseStream::Skip( int bytes )
{
    assert( bytes >= 0 );
    m_current += bytes;
}

jmp_buf& RBaseStream::JmpBuf()
{ 
    m_jmp_set = true;
    return m_jmp_buf;
}

/////////////////////////  RLByteStream ////////////////////////////

RLByteStream::~RLByteStream()
{
}

int  RLByteStream::GetByte()
{
    uchar *current = m_current;
    int   val;

    if( current >= m_end )
    {
        ReadBlock();
        current = m_current;
    }

    val = *((uchar*)current);
    m_current = current + 1;
    return val;
}


void  RLByteStream::GetBytes( void* buffer, int count, int* readed )
{
    uchar*  data = (uchar*)buffer;
    assert( count >= 0 );
    
    if( readed) *readed = 0;

    while( count > 0 )
    {
        int l;

        for(;;)
        {
            l = (int)(m_end - m_current);
            if( l > count ) l = count;
            if( l > 0 ) break;
            ReadBlock();
        }
        memcpy( data, m_current, l );
        m_current += l;
        data += l;
        count -= l;
        if( readed ) *readed += l;
    }
}


////////////  RLByteStream & RMByteStream <Get[d]word>s ////////////////

RMByteStream::~RMByteStream()
{
}


int  RLByteStream::GetWord()
{
    uchar *current = m_current;
    int   val;

    if( current+1 < m_end )
    {
        val = current[0] + (current[1] << 8);
        m_current = current + 2;
    }
    else
    {
        val = GetByte();
        val|= GetByte() << 8;
    }
    return val;
}


int  RLByteStream::GetDWord()
{
    uchar *current = m_current;
    int   val;

    if( current+3 < m_end )
    {
        val = current[0] + (current[1] << 8) +
              (current[2] << 16) + (current[3] << 24);
        m_current = current + 4;
    }
    else
    {
        val = GetByte();
        val |= GetByte() << 8;
        val |= GetByte() << 16;
        val |= GetByte() << 24;
    }
    return val;
}


int  RMByteStream::GetWord()
{
    uchar *current = m_current;
    int   val;

    if( current+1 < m_end )
    {
        val = (current[0] << 8) + current[1];
        m_current = current + 2;
    }
    else
    {
        val = GetByte() << 8;
        val|= GetByte();
    }
    return val;
}


int  RMByteStream::GetDWord()
{
    uchar *current = m_current;
    int   val;

    if( current+3 < m_end )
    {
        val = (current[0] << 24) + (current[1] << 16) +
              (current[2] << 8) + current[3];
        m_current = current + 4;
    }
    else
    {
        val = GetByte() << 24;
        val |= GetByte() << 16;
        val |= GetByte() << 8;
        val |= GetByte();
    }
    return val;
}


/////////////////////////  RLBitStream ////////////////////////////

RLBitStream::~RLBitStream()
{
}


void  RLBitStream::ReadBlock()
{
    RBaseStream::ReadBlock();
    if( bsIsBigEndian() )
        bsBSwapBlock( m_start, m_end );
}


void  RLBitStream::SetPos( int pos )
{
    RBaseStream::SetPos(pos);
    int offset = (int)(m_current - m_end);
    m_current = m_end + (offset & -4);
    m_bit_idx = (offset&3)*8;
}


int  RLBitStream::GetPos()
{
    return RBaseStream::GetPos() + (m_bit_idx >> 3);
}


int  RLBitStream::Get( int bits )
{
    int    bit_idx     = m_bit_idx;
    int    new_bit_idx = bit_idx + bits;
    int    mask    = new_bit_idx >= 32 ? -1 : 0;
    unsigned* current = (unsigned*)m_current;

    assert( (unsigned)bits < 32 );

    if( (m_current = (uchar*)(current - mask)) >= m_end )
    {
        ReadBlock();
        current = ((unsigned*)m_current) + mask;
    }
    m_bit_idx = new_bit_idx & 31;
    return ((current[0] >> bit_idx) |
           ((current[1] <<-bit_idx) & mask)) & bs_bit_mask[bits];
}

int  RLBitStream::Show( int bits )
{
    int    bit_idx = m_bit_idx;
    int    new_bit_idx = bit_idx + bits;
    int    mask    = new_bit_idx >= 32 ? -1 : 0;
    unsigned* current = (unsigned*)m_current;

    assert( (unsigned)bits < 32 );

    if( (uchar*)(current - mask) >= m_end )
    {
        ReadBlock();
        current = ((unsigned*)m_current) + mask;
        m_current = (uchar*)current;
    }
    return ((current[0] >> bit_idx) |
           ((current[1] <<-bit_idx) & mask)) & bs_bit_mask[bits];
}


void  RLBitStream::Move( int shift )
{
    int new_bit_idx = m_bit_idx + shift;
    m_current += (new_bit_idx >> 5) << 2;
    m_bit_idx  = new_bit_idx & 31;
}


int  RLBitStream::GetHuff( const short* table )
{
    int  val;
    int  code_bits;

    for(;;)
    {
        int table_bits = table[0];
        val = table[Show(table_bits) + 2];
        code_bits = val & 15;
        val >>= 4;

        if( code_bits != 0 ) break;
        table += val*2;
        Move( table_bits );
    }

    Move( code_bits );
    if( val == RBS_HUFF_FORB )
    {
        if( m_jmp_set )
            longjmp( m_jmp_buf, RBS_THROW_FORB );
    }

    return val;
}

void  RLBitStream::Skip( int bytes )
{
    Move( bytes*8 );
}

/////////////////////////  RMBitStream ////////////////////////////


RMBitStream::~RMBitStream()
{
}


void  RMBitStream::ReadBlock()
{
    RBaseStream::ReadBlock();
    if( !bsIsBigEndian() )
        bsBSwapBlock( m_start, m_end );
}


void  RMBitStream::SetPos( int pos )
{
    RBaseStream::SetPos(pos);
    int offset = (int)(m_current - m_end);
    m_current = m_end + ((offset - 1) & -4);
    m_bit_idx = (32 - (offset&3)*8) & 31;
}


int  RMBitStream::GetPos()
{
    return RBaseStream::GetPos() + ((32 - m_bit_idx) >> 3);
}


int  RMBitStream::Get( int bits )
{
    int    bit_idx = m_bit_idx - bits;
    int    mask    = bit_idx >> 31;
    unsigned* current = ((unsigned*)m_current) - mask;

    assert( (unsigned)bits < 32 );

    if( (m_current = (uchar*)current) >= m_end )
    {
        ReadBlock();
        current = (unsigned*)m_current;
    }
    m_bit_idx = bit_idx &= 31;
    return (((current[-1] << -bit_idx) & mask)|
             (current[0] >> bit_idx)) & bs_bit_mask[bits];
}


int  RMBitStream::Show( int bits )
{
    int    bit_idx = m_bit_idx - bits;
    int    mask    = bit_idx >> 31;
    unsigned* current = ((unsigned*)m_current) - mask;

    assert( (unsigned)bits < 32 );

    if( ((uchar*)current) >= m_end )
    {
        m_current = (uchar*)current;
        ReadBlock();
        current = (unsigned*)m_current;
        m_current -= 4;
    }
    return (((current[-1]<<-bit_idx) & mask)|
             (current[0] >> bit_idx)) & bs_bit_mask[bits];
}


int  RMBitStream::GetHuff( const short* table )
{
    int  val;
    int  code_bits;

    for(;;)
    {
        int table_bits = table[0];
        val = table[Show(table_bits) + 1];
        code_bits = val & 15;
        val >>= 4;

        if( code_bits != 0 ) break;
        table += val;
        Move( table_bits );
    }

    Move( code_bits );
    if( val == RBS_HUFF_FORB )
    {
        if( m_jmp_set )
            longjmp( m_jmp_buf, RBS_THROW_FORB );
    }

    return val;
}


void  RMBitStream::Move( int shift )
{
    int new_bit_idx = m_bit_idx - shift;
    m_current -= (new_bit_idx >> 5)<<2;
    m_bit_idx  = new_bit_idx & 31;
}


void  RMBitStream::Skip( int bytes )
{
    Move( bytes*8 );
}


static const int huff_val_shift = 20, huff_code_mask = (1 << huff_val_shift) - 1;

bool bsCreateDecodeHuffmanTable( const int* src, short* table, int max_size )
{   
    const int forbidden_entry = (RBS_HUFF_FORB << 4)|1;
    int       first_bits = src[0];
    struct
    {
        int bits;
        int offset;
    }
    sub_tables[1 << 11];
    int  size = (1 << first_bits) + 1;
    int  i, k;
    
    /* calc bit depths of sub tables */
    memset( sub_tables, 0, ((size_t)1 << first_bits)*sizeof(sub_tables[0]) );
    for( i = 1, k = 1; src[k] >= 0; i++ )
    {
        int code_count = src[k++];
        int sb = i - first_bits;
        
        if( sb <= 0 )
            k += code_count;
        else
            for( code_count += k; k < code_count; k++ )
            {
                int  code = src[k] & huff_code_mask;
                sub_tables[code >> sb].bits = sb;
            }
    }

    /* calc offsets of sub tables and whole size of table */
    for( i = 0; i < (1 << first_bits); i++ )
    {
        int b = sub_tables[i].bits;
        if( b > 0 )
        {
            b = 1 << b;
            sub_tables[i].offset = size;
            size += b + 1;
        }
    }

    if( size > max_size )
    {
        assert(0);
        return false;
    }

    /* fill first table and subtables with forbidden values */
    for( i = 0; i < size; i++ )
    {
        table[i] = (short)forbidden_entry;
    }

    /* write header of first table */
    table[0] = (short)first_bits;

    /* fill first table and sub tables */ 
    for( i = 1, k = 1; src[k] >= 0; i++ )
    {
        int code_count = src[k++];
        for( code_count += k; k < code_count; k++ )
        {
            int  table_bits= first_bits;
            int  code_bits = i;
            int  code = src[k] & huff_code_mask;
            int  val  = src[k] >>huff_val_shift;
            int  j, offset = 0;

            if( code_bits > table_bits )
            {
                int idx = code >> (code_bits -= table_bits);
                code &= (1 << code_bits) - 1;
                offset   = sub_tables[idx].offset;
                table_bits= sub_tables[idx].bits;
                /* write header of subtable */
                table[offset]  = (short)table_bits;
                /* write jump to subtable */
                table[idx + 1]= (short)(offset << 4);
            }
        
            table_bits -= code_bits;
            assert( table_bits >= 0 );
            val = (val << 4) | code_bits;
            offset += (code << table_bits) + 1;
        
            for( j = 0; j < (1 << table_bits); j++ )
            {
                assert( table[offset + j] == forbidden_entry );
                table[ offset + j ] = (short)val;
            }
        }
    }
    return true;
}


int*  bsCreateSourceHuffmanTable( const uchar* src, int* dst,
                                  int max_bits, int first_bits )
{
    int   i, val_idx, code = 0;
    int*  table = dst;
    *dst++ = first_bits;
    for( i = 1, val_idx = max_bits; i <= max_bits; i++ )
    {
        int code_count = src[i - 1];
        dst[0] = code_count;
        code <<= 1;
        for( int k = 0; k < code_count; k++ )
        {
            dst[k + 1] = (src[val_idx + k] << huff_val_shift)|(code + k);
        }
        code += code_count;
        dst += code_count + 1;
        val_idx += code_count;
    }
    dst[0] = -1;
    return  table;
}


/////////////////////////// WBaseStream /////////////////////////////////

// WBaseStream - base class for output streams
WBaseStream::WBaseStream()
{
    m_start = m_end = m_current = 0;
    m_file = 0;
    m_block_size = BS_DEF_BLOCK_SIZE;
    m_is_opened = false;
}


WBaseStream::~WBaseStream()
{
    Close();    // Close files
    Release();  // free  buffers
}


bool  WBaseStream::IsOpened()
{ 
    return m_is_opened;
}


void  WBaseStream::Allocate()
{
    if( !m_start )
        m_start = &m_buf[0];

    m_end = m_start + m_block_size;
    m_current = m_start;
}


void  WBaseStream::WriteBlock()
{
    int size = (int)(m_current - m_start);
    assert( m_file != 0 );

    //fseek( m_file, m_block_pos, SEEK_SET );
    if( size > 0 )
        fwrite( m_start, 1, size, m_file );
    m_current = m_start;

    /*if( written < size ) throw RBS_THROW_EOS;*/
    
    m_block_pos += size;
}


bool  WBaseStream::Open( const char* filename )
{
    Close();
    Allocate();
    
    m_file = fopen( filename, "wb" );
    
    if( m_file )
    {
        m_is_opened = true;
        m_block_pos = 0;
        m_current = m_start;
    }
    return m_file != 0;
}


void  WBaseStream::Close()
{
    if( m_file )
    {
        WriteBlock();
        fclose( m_file );
        m_file = 0;
    }
    m_is_opened = false;
}


void  WBaseStream::Release()
{
    m_start = m_end = m_current = 0;
}


void  WBaseStream::SetBlockSize( int block_size )
{
    assert( block_size > 0 && (block_size & (block_size-1)) == 0 );

    if( m_start && block_size == m_block_size ) return;
    Release();
    m_block_size = BS_DEF_BLOCK_SIZE;
    Allocate();
}


int  WBaseStream::GetPos()
{
    assert( IsOpened() );
    return m_block_pos + (int)(m_current - m_start);
}


///////////////////////////// WLByteStream /////////////////////////////////// 

WLByteStream::~WLByteStream()
{
}

void WLByteStream::PutByte( int val )
{
    *m_current++ = (uchar)val;
    if( m_current >= m_end )
        WBaseStream::WriteBlock();
}


void WLByteStream::PutBytes( const void* buffer, int count )
{
    uchar* data = (uchar*)buffer;
    
    assert( data && m_current && count >= 0 );

    while( count > 0 )
    {
        int l = (int)(m_end - m_current);
        
        if( l > count )
            l = count;
        
        if( l > 0 )
        {
            memcpy( m_current, data, l );
            m_current += l;
            data += l;
            count -= l;
        }
        if( m_current >= m_end )
            WriteBlock();
    }
}


void WLByteStream::PutWord( int val )
{
    uchar *current = m_current;

    if( current+1 < m_end )
    {
        current[0] = (uchar)val;
        current[1] = (uchar)(val >> 8);
        m_current = current + 2;
        if( m_current == m_end )
            WriteBlock();
    }
    else
    {
        PutByte(val);
        PutByte(val >> 8);
    }
}


void WLByteStream::PutDWord( int val )
{
    uchar *current = m_current;

    if( current+3 < m_end )
    {
        current[0] = (uchar)val;
        current[1] = (uchar)(val >> 8);
        current[2] = (uchar)(val >> 16);
        current[3] = (uchar)(val >> 24);
        m_current = current + 4;
        if( m_current == m_end )
            WriteBlock();
    }
    else
    {
        PutByte(val);
        PutByte(val >> 8);
        PutByte(val >> 16);
        PutByte(val >> 24);
    }
}


///////////////////////////// WMByteStream /////////////////////////////////// 

WMByteStream::~WMByteStream()
{
}


void WMByteStream::PutWord( int val )
{
    uchar *current = m_current;

    if( current+1 < m_end )
    {
        current[0] = (uchar)(val >> 8);
        current[1] = (uchar)val;
        m_current = current + 2;
        if( m_current == m_end )
            WriteBlock();
    }
    else
    {
        PutByte(val >> 8);
        PutByte(val);
    }
}


void WMByteStream::PutDWord( int val )
{
    uchar *current = m_current;

    if( current+3 < m_end )
    {
        current[0] = (uchar)(val >> 24);
        current[1] = (uchar)(val >> 16);
        current[2] = (uchar)(val >> 8);
        current[3] = (uchar)val;
        m_current = current + 4;
        if( m_current == m_end )
            WriteBlock();
    }
    else
    {
        PutByte(val >> 24);
        PutByte(val >> 16);
        PutByte(val >> 8);
        PutByte(val);
    }
}


///////////////////////////// WMBitStream /////////////////////////////////// 

WMBitStream::WMBitStream()
{
    m_pad_val = 0;
    ResetBuffer();
}


WMBitStream::~WMBitStream()
{
}


bool  WMBitStream::Open( const char* filename )
{
    ResetBuffer();
    return WBaseStream::Open( filename );
}


void  WMBitStream::ResetBuffer()
{
    m_val = 0;
    m_bit_idx = 32;
    m_current = m_start;
}

void  WMBitStream::Flush()
{
    if( m_bit_idx < 32 )
    {
        Put( m_pad_val, m_bit_idx & 7 );
        *((unsigned*&)m_current)++ = m_val;
    }
}


void  WMBitStream::Close()
{
    if( m_is_opened )
    {
        Flush();
        WBaseStream::Close();
    }
}


void  WMBitStream::WriteBlock()
{
    if( !bsIsBigEndian() )
        bsBSwapBlock( m_start, m_current );
    WBaseStream::WriteBlock();
}


int  WMBitStream::GetPos()
{
    return WBaseStream::GetPos() + ((32 - m_bit_idx) >> 3);
}


void  WMBitStream::Put( int val, int bits )
{
    int  bit_idx = m_bit_idx - bits;
    unsigned  curval = m_val;

    CV_Assert( 0 <= bits && bits < 32 );

    val &= bs_bit_mask[bits];

    if( bit_idx >= 0 )
    {
        curval |= val << bit_idx;
    }
    else
    {
        *((unsigned*)m_current) = curval | ((unsigned)val >> -bit_idx);
        m_current += sizeof(unsigned);
        if( m_current >= m_end )
        {
            WriteBlock();
        }
        bit_idx += 32;
        curval = val << bit_idx;
    }

    m_val = curval;
    m_bit_idx = bit_idx;
}


void  WMBitStream::PutHuff( int val, const unsigned* table )
{
    int min_val = (int)table[0];
    val -= min_val;
    
    assert( (unsigned)val < table[1] );

    unsigned code = table[val + 2];
    assert( code != 0 );
    
    Put( code >> 8, code & 255 );
}


bool bsCreateEncodeHuffmanTable( const int* src, unsigned* table, int max_size )
{   
    int  i, k;
    int  min_val = INT_MAX, max_val = INT_MIN;
    int  size;
    
    /* calc min and max values in the table */
    for( i = 1, k = 1; src[k] >= 0; i++ )
    {
        int code_count = src[k++];

        for( code_count += k; k < code_count; k++ )
        {
            int  val = src[k] >> huff_val_shift;
            if( val < min_val )
                min_val = val;
            if( val > max_val )
                max_val = val;
        }
    }

    size = max_val - min_val + 3;

    if( size > max_size )
    {
        assert(0);
        return false;
    }

    memset( table, 0, size*sizeof(table[0]));

    table[0] = min_val;
    table[1] = size - 2;

    for( i = 1, k = 1; src[k] >= 0; i++ )
    {
        int code_count = src[k++];

        for( code_count += k; k < code_count; k++ )
        {
            int  val = src[k] >> huff_val_shift;
            int  code = src[k] & huff_code_mask;

            table[val - min_val + 2] = (code << 8) | i;
        }
    }
    return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////

class RJpegBitStream : public RMBitStream
{
public:
    RMByteStream  m_low_strm;

    RJpegBitStream();
    ~RJpegBitStream();

    virtual bool  Open( const char* filename );
    virtual void  Close();

    void  Flush(); // flushes high-level bit stream
    void  AlignOnByte();
    int   FindMarker();

protected:
    virtual void  ReadBlock();
};


//////////////////// JPEG reader /////////////////////

class GrFmtJpegReader
{
public:

    GrFmtJpegReader( const char* filename );
    ~GrFmtJpegReader();

    bool  ReadData( uchar* data, int step, int color );
    bool  ReadHeader();
    void  Close();
    int m_width, m_height, m_iscolor;

protected:

    int   m_offset; // offset of first scan
    int   m_version; // JFIF version
    int   m_planes; // 3 (YCrCb) or 1 (Gray)
    int   m_precision; // 8 or 12-bit per sample
    int   m_type; // SOF type
    int   m_MCUs; // MCUs in restart interval
    int   m_ss, m_se, m_ah, m_al; // progressive JPEG parameters

    // information about each component
    struct cmp_info
    {
        char h;  // horizontal sampling factor
        char v;  // vertical   sampling factor
        char tq; // quantization table index
        char td, ta; // DC & AC huffman tables
        int  dc_pred; // DC predictor
    };

    cmp_info m_ci[3];

    int     m_tq[4][64];
    bool    m_is_tq[4];

    short*  m_td[4];
    bool    m_is_td[4];

    short*  m_ta[4];
    bool    m_is_ta[4];
    
    RJpegBitStream  m_strm;
    const char*   m_filename;

protected:
    
    bool  LoadQuantTables( int length );
    bool  LoadHuffmanTables( int length );
    void  ProcessScan( int* idx, int ns, uchar* data, int step, int color );
    void  ResetDecoder();
    void  GetBlock( int* block, int c );
};

RJpegBitStream::RJpegBitStream()
{
}

RJpegBitStream::~RJpegBitStream()
{
    Close();
}


bool  RJpegBitStream::Open( const char* filename )
{
    Close();
    Allocate();

    m_is_opened = m_low_strm.Open( filename );
    if( m_is_opened ) SetPos(0);
    return m_is_opened;
}


void  RJpegBitStream::Close()
{
    m_low_strm.Close();
    m_is_opened = false;
}


void  RJpegBitStream::ReadBlock()
{
    uchar* end = m_start + m_block_size;
    uchar* current = m_start;

    if( setjmp( m_low_strm.JmpBuf()) == 0 )
    {
        int sz = m_unGetsize;
        memmove( current - sz, m_end - sz, sz );
        while( current < end )
        {
            int val = m_low_strm.GetByte();
            if( val != 0xff )
            {
                *current++ = (uchar)val;
            }
            else
            {
                val = m_low_strm.GetByte();
                if( val == 0 )
                    *current++ = 0xFF;
                else if( !(0xD0 <= val && val <= 0xD7) )
                {
                    m_low_strm.SetPos( m_low_strm.GetPos() - 2 );
                    goto fetch_end;
                }
            }
        }
    fetch_end: ;
    }
    else
    {
        if( current == m_start && m_jmp_set )
            longjmp( m_jmp_buf, RBS_THROW_EOS );
    }
    m_current = m_start;
    m_end = m_start + (((current - m_start) + 3) & -4);
    if( !bsIsBigEndian() )
        bsBSwapBlock( m_start, m_end );
}


void  RJpegBitStream::Flush()
{
    m_end = m_start + m_block_size;
    m_current = m_end - 4;
    m_bit_idx = 0;
}

void  RJpegBitStream::AlignOnByte()
{
    m_bit_idx &= -8;
}

int  RJpegBitStream::FindMarker()
{
    int code = m_low_strm.GetWord();
    while( (code & 0xFF00) != 0xFF00 || (code == 0xFFFF || code == 0xFF00 ))
    {
        code = ((code&255) << 8) | m_low_strm.GetByte();
    }
    return code;
}


// IDCT without prescaling
static void aan_idct8x8( int *src, int *dst, int step )
{
    int   workspace[64], *work = workspace;
    int   i;

    /* Pass 1: process rows */
    for( i = 8; i > 0; i--, src += 8, work += 8 )
    {
        /* Odd part */
        int  x0 = src[5], x1 = src[3];
        int  x2 = src[1], x3 = src[7];

        int  x4 = x0 + x1; x0 -= x1;

        x1 = x2 + x3; x2 -= x3;
        x3 = x1 + x4; x1 -= x4;

        x4 = (x0 + x2)*C1_847;
        x0 = descale( x4 - x0*C2_613, fixb);
        x2 = descale( x2*C1_082 - x4, fixb);
        x1 = descale( x1*C1_414, fixb);

        x0 -= x3;
        x1 -= x0;
        x2 += x1;

        work[7] = x3; work[6] = x0;
        work[5] = x1; work[4] = x2;

        /* Even part */
        x2 = src[2]; x3 = src[6];
        x0 = src[0]; x1 = src[4];

        x4 = x2 + x3;
        x2 = descale((x2-x3)*C1_414, fixb) - x4;

        x3 = x0 + x1; x0 -= x1;
        x1 = x3 + x4; x3 -= x4;
        x4 = x0 + x2; x0 -= x2;

        x2 = work[7];
        x1 -= x2; x2 = 2*x2 + x1;
        work[7] = x1; work[0] = x2;

        x2 = work[6];
        x1 = x4 + x2; x4 -= x2;
        work[1] = x1; work[6] = x4;

        x1 = work[5]; x2 = work[4];
        x4 = x0 + x1; x0 -= x1;
        x1 = x3 + x2; x3 -= x2;

        work[2] = x4; work[5] = x0;
        work[3] = x3; work[4] = x1;
    }

    /* Pass 2: process columns */
    work = workspace;
    for( i = 8; i > 0; i--, dst += step, work++ )
    {
        /* Odd part */
        int  x0 = work[8*5], x1 = work[8*3];
        int  x2 = work[8*1], x3 = work[8*7];

        int  x4 = x0 + x1; x0 -= x1;
        x1 = x2 + x3; x2 -= x3;
        x3 = x1 + x4; x1 -= x4;

        x4 = (x0 + x2)*C1_847;
        x0 = descale( x4 - x0*C2_613, fixb);
        x2 = descale( x2*C1_082 - x4, fixb);
        x1 = descale( x1*C1_414, fixb);

        x0 -= x3;
        x1 -= x0;
        x2 += x1;

        dst[7] = x3; dst[6] = x0;
        dst[5] = x1; dst[4] = x2;

        /* Even part */
        x2 = work[8*2]; x3 = work[8*6];
        x0 = work[8*0]; x1 = work[8*4];

        x4 = x2 + x3;
        x2 = descale((x2-x3)*C1_414, fixb) - x4;

        x3 = x0 + x1; x0 -= x1;
        x1 = x3 + x4; x3 -= x4;
        x4 = x0 + x2; x0 -= x2;

        x2 = dst[7];
        x1 -= x2; x2 = 2*x2 + x1;
        x1 = descale(x1,3);
        x2 = descale(x2,3);

        dst[7] = x1; dst[0] = x2;

        x2 = dst[6];
        x1 = descale(x4 + x2,3);
        x4 = descale(x4 - x2,3);
        dst[1] = x1; dst[6] = x4;

        x1 = dst[5]; x2 = dst[4];

        x4 = descale(x0 + x1,3);
        x0 = descale(x0 - x1,3);
        x1 = descale(x3 + x2,3);
        x3 = descale(x3 - x2,3);

        dst[2] = x4; dst[5] = x0;
        dst[3] = x3; dst[4] = x1;
    }
}


static const int max_dec_htable_size = 1 << 12;
static const int first_table_bits = 9;

GrFmtJpegReader::GrFmtJpegReader( const char* filename )
{
    m_filename = filename;
    m_planes= -1;
    m_offset= -1;

    int i;
    for( i = 0; i < 4; i++ )
    {
        m_td[i] = new short[max_dec_htable_size];
        m_ta[i] = new short[max_dec_htable_size];
    }
}


GrFmtJpegReader::~GrFmtJpegReader()
{
    for( int i = 0; i < 4; i++ )
    {
        delete[] m_td[i];
        m_td[i] = 0;
        delete[] m_ta[i];
        m_ta[i] = 0;
    }
}


void  GrFmtJpegReader::Close()
{
    m_strm.Close();
}


bool GrFmtJpegReader::ReadHeader()
{
    char buffer[16];
    int  i;
    bool result = false, is_sof = false,
    is_qt = false, is_ht = false, is_sos = false;

    assert( strlen(m_filename) != 0 );
    if( !m_strm.Open( m_filename )) return false;

    memset( m_is_tq, 0, sizeof(m_is_tq));
    memset( m_is_td, 0, sizeof(m_is_td));
    memset( m_is_ta, 0, sizeof(m_is_ta));
    m_MCUs = 0;

    if( setjmp( m_strm.JmpBuf()) == 0 )
    {
        RMByteStream& lstrm = m_strm.m_low_strm;

        lstrm.Skip( 2 ); // skip SOI marker

        for(;;)
        {
            int marker = m_strm.FindMarker() & 255;

            // check for standalone markers
            if( marker != 0xD8 /* SOI */ && marker != 0xD9 /* EOI */ &&
               marker != 0x01 /* TEM */ && !( 0xD0 <= marker && marker <= 0xD7 ))
            {
                int pos    = lstrm.GetPos();
                int length = lstrm.GetWord();

                switch( marker )
                {
                    case 0xE0: // APP0
                        lstrm.GetBytes( buffer, 5 );
                        if( strcmp(buffer, "JFIF") == 0 ) // JFIF identification
                        {
                            m_version = lstrm.GetWord();
                            //is_jfif = true;
                        }
                        break;

                    case 0xC0: // SOF0
                        m_precision = lstrm.GetByte();
                        m_height = lstrm.GetWord();
                        m_width = lstrm.GetWord();
                        m_planes = lstrm.GetByte();

                        if( m_width == 0 || m_height == 0 || // DNL not supported
                           (m_planes != 1 && m_planes != 3)) goto parsing_end;

                        m_iscolor = m_planes == 3;

                        memset( m_ci, -1, sizeof(m_ci));

                        for( i = 0; i < m_planes; i++ )
                        {
                            int idx = lstrm.GetByte();

                            if( idx < 1 || idx > m_planes ) // wrong index
                            {
                                idx = i+1; // hack
                            }
                            cmp_info& ci = m_ci[idx-1];

                            if( ci.tq > 0 /* duplicated description */) goto parsing_end;

                            ci.h = (char)lstrm.GetByte();
                            ci.v = (char)(ci.h & 15);
                            ci.h >>= 4;
                            ci.tq = (char)lstrm.GetByte();
                            if( !((ci.h == 1 || ci.h == 2 || ci.h == 4) &&
                                  (ci.v == 1 || ci.v == 2 || ci.v == 4) &&
                                  ci.tq < 3) ||
                               // chroma mcu-parts should have equal sizes and
                               // be non greater then luma sizes
                               !( i != 2 || (ci.h == m_ci[1].h && ci.v == m_ci[1].v &&
                                             ci.h <= m_ci[0].h && ci.v <= m_ci[0].v)))
                                goto parsing_end;
                        }
                        is_sof = true;
                        m_type = marker - 0xC0;
                        break;

                    case 0xDB: // DQT
                        if( !LoadQuantTables( length )) goto parsing_end;
                        is_qt = true;
                        break;

                    case 0xC4: // DHT
                        if( !LoadHuffmanTables( length )) goto parsing_end;
                        is_ht = true;
                        break;

                    case 0xDA: // SOS
                        is_sos = true;
                        m_offset = pos - 2;
                        goto parsing_end;

                    case 0xDD: // DRI
                        m_MCUs = lstrm.GetWord();
                        break;
                }
                lstrm.SetPos( pos + length );
            }
        }
    parsing_end: ;
    }

    result = /*is_jfif &&*/ is_sof && is_qt && is_ht && is_sos;
    if( !result )
    {
        m_width = m_height = -1;
        m_offset = -1;
        m_strm.Close();
    }
    return result;
}


bool GrFmtJpegReader::LoadQuantTables( int length )
{
    uchar buffer[128];
    int  i, tq_size;

    RMByteStream& lstrm = m_strm.m_low_strm;
    length -= 2;

    while( length > 0 )
    {
        int tq = lstrm.GetByte();
        int size = tq >> 4;
        tq &= 15;

        tq_size = (64<<size) + 1;
        if( tq > 3 || size > 1 || length < tq_size ) return false;
        length -= tq_size;

        lstrm.GetBytes( buffer, tq_size - 1 );

        if( size == 0 ) // 8 bit quant factors
        {
            for( i = 0; i < 64; i++ )
            {
                int idx = zigzag[i];
                m_tq[tq][idx] = buffer[i] * 16 * idct_prescale[idx];
            }
        }
        else // 16 bit quant factors
        {
            for( i = 0; i < 64; i++ )
            {
                int idx = zigzag[i];
                m_tq[tq][idx] = ((unsigned short*)buffer)[i] * idct_prescale[idx];
            }
        }
        m_is_tq[tq] = true;
    }

    return true;
}


bool GrFmtJpegReader::LoadHuffmanTables( int length )
{
    const int max_bits = 16;
    uchar buffer[1024];
    int  buffer2[1024];

    int  i, ht_size;
    RMByteStream& lstrm = m_strm.m_low_strm;
    length -= 2;

    while( length > 0 )
    {
        int t = lstrm.GetByte();
        int hclass = t >> 4;
        t &= 15;

        if( t > 3 || hclass > 1 || length < 17 ) return false;
        length -= 17;

        lstrm.GetBytes( buffer, max_bits );
        for( i = 0, ht_size = 0; i < max_bits; i++ ) ht_size += buffer[i];

        if( length < ht_size ) return false;
        length -= ht_size;

        lstrm.GetBytes( buffer + max_bits, ht_size );

        if( !bsCreateDecodeHuffmanTable(bsCreateSourceHuffmanTable(
                    buffer, buffer2, max_bits, first_table_bits ),
                    hclass == 0 ? m_td[t] : m_ta[t],
                    max_dec_htable_size )) return false;
        if( hclass == 0 )
            m_is_td[t] = true;
        else
            m_is_ta[t] = true;
    }
    return true;
}


bool GrFmtJpegReader::ReadData( uchar* data, int step, int color )
{
    if( m_offset < 0 || !m_strm.IsOpened())
        return false;

    if( setjmp( m_strm.JmpBuf()) == 0 )
    {
        RMByteStream& lstrm = m_strm.m_low_strm;
        lstrm.SetPos( m_offset );

        for(;;)
        {
            int marker = m_strm.FindMarker() & 255;

            if( marker == 0xD8 /* SOI */ || marker == 0xD9 /* EOI */ )
                goto decoding_end;

            // check for standalone markers
            if( marker != 0x01 /* TEM */ && !( 0xD0 <= marker && marker <= 0xD7 ))
            {
                int pos    = lstrm.GetPos();
                int length = lstrm.GetWord();

                switch( marker )
                {
                    case 0xC4: // DHT
                        if( !LoadHuffmanTables( length )) goto decoding_end;
                        break;

                    case 0xDA: // SOS
                               // read scan header
                    {
                        int idx[3] = { -1, -1, -1 };
                        int i, ns = lstrm.GetByte();
                        int sum = 0, a; // spectral selection & approximation

                        if( ns != m_planes ) goto decoding_end;
                        for( i = 0; i < ns; i++ )
                        {
                            int td, ta, c = lstrm.GetByte() - 1;
                            if( c < 0 || m_planes <= c )
                            {
                                c = i; // hack
                            }

                            if( idx[c] != -1 ) goto decoding_end;
                            idx[i] = c;
                            td = lstrm.GetByte();
                            ta = td & 15;
                            td >>= 4;
                            if( !(ta <= 3 && m_is_ta[ta] &&
                                  td <= 3 && m_is_td[td] &&
                                  m_is_tq[m_ci[c].tq]) )
                                goto decoding_end;

                            m_ci[c].td = (char)td;
                            m_ci[c].ta = (char)ta;

                            sum += m_ci[c].h*m_ci[c].v;
                        }

                        if( sum > 10 ) goto decoding_end;

                        m_ss = lstrm.GetByte();
                        m_se = lstrm.GetByte();

                        a = lstrm.GetByte();
                        m_al = a & 15;
                        m_ah = a >> 4;

                        ProcessScan( idx, ns, data, step, color );
                        goto decoding_end; // only single scan case is supported now
                    }

                        //m_offset = pos - 2;
                        //break;

                    case 0xDD: // DRI
                        m_MCUs = lstrm.GetWord();
                        break;
                }

                if( marker != 0xDA ) lstrm.SetPos( pos + length );
            }
        }
    decoding_end: ;
    }

    return true;
}


void  GrFmtJpegReader::ResetDecoder()
{
    m_ci[0].dc_pred = m_ci[1].dc_pred = m_ci[2].dc_pred = 0;
}

void  GrFmtJpegReader::ProcessScan( int* idx, int ns, uchar* data, int step, int color )
{
    int   i, s = 0, mcu, x1 = 0, y1 = 0;
    int   temp[64];
    int   blocks[10][64];
    int   pos[3], h[3], v[3];
    int   x_shift = 0, y_shift = 0;
    int   nch = color ? 3 : 1;

    assert( ns == m_planes && m_ss == 0 && m_se == 63 &&
           m_al == 0 && m_ah == 0 ); // sequental & single scan

    assert( idx[0] == 0 && (ns ==1 || (idx[1] == 1 && idx[2] == 2)));

    for( i = 0; i < ns; i++ )
    {
        int c = idx[i];
        h[c] = m_ci[c].h*8;
        v[c] = m_ci[c].v*8;
        pos[c] = s >> 6;
        s += h[c]*v[c];
    }

    if( ns == 3 )
    {
        x_shift = h[0]/(h[1]*2);
        y_shift = v[0]/(v[1]*2);
    }

    m_strm.Flush();
    ResetDecoder();

    for( mcu = 0;; mcu++ )
    {
        int  x2, y2, x, y, xc;
        int* cmp;
        uchar* data1;

        if( mcu == m_MCUs && m_MCUs != 0 )
        {
            ResetDecoder();
            m_strm.AlignOnByte();
            mcu = 0;
        }

        // Get mcu
        for( i = 0; i < ns; i++ )
        {
            int  c = idx[i];
            cmp = blocks[pos[c]];
            for( y = 0; y < v[c]; y += 8, cmp += h[c]*8 )
                for( x = 0; x < h[c]; x += 8 )
                {
                    GetBlock( temp, c );
                    if( i < (color ? 3 : 1))
                    {
                        aan_idct8x8( temp, cmp + x, h[c] );
                    }
                }
        }

        y2 = v[0];
        x2 = h[0];

        if( y1 + y2 > m_height ) y2 = m_height - y1;
        if( x1 + x2 > m_width ) x2 = m_width - x1;

        cmp = blocks[0];
        data1 = data + x1*nch;

        if( ns == 1 )
            for( y = 0; y < y2; y++, data1 += step, cmp += h[0] )
            {
                if( color )
                {
                    for( x = 0; x < x2; x++ )
                    {
                        int val = descale( cmp[x] + 128*4, 2 );
                        data1[x*3] = data1[x*3 + 1] = data1[x*3 + 2] = saturate( val );
                    }
                }
                else
                {
                    for( x = 0; x < x2; x++ )
                    {
                        int val = descale( cmp[x] + 128*4, 2 );
                        data1[x] = saturate( val );
                    }
                }
            }
        else
        {
            for( y = 0; y < y2; y++, data1 += step, cmp += h[0] )
            {
                if( color )
                {
                    int  shift = h[1]*(y >> y_shift);
                    int* cmpCb = blocks[pos[1]] + shift;
                    int* cmpCr = blocks[pos[2]] + shift;
                    x = 0;
                    if( x_shift == 0 )
                    {
                        for( ; x < x2; x++ )
                        {
                            int Y  = (cmp[x] + 128*4) << fixc;
                            int Cb = cmpCb[x];
                            int Cr = cmpCr[x];
                            int t = (Y + Cb*b_cb) >> (fixc + 2);
                            data1[x*3] = saturate(t);
                            t = (Y + Cb*g_cb + Cr*g_cr) >> (fixc + 2);
                            data1[x*3 + 1] = saturate(t);
                            t = (Y + Cr*r_cr) >> (fixc + 2);
                            data1[x*3 + 2] = saturate(t);
                        }
                    }
                    else if( x_shift == 1 )
                    {
                        for( xc = 0; x <= x2 - 2; x += 2, xc++ )
                        {
                            int Y  = (cmp[x] + 128*4) << fixc;
                            int Cb = cmpCb[xc];
                            int Cr = cmpCr[xc];
                            int t = (Y + Cb*b_cb) >> (fixc + 2);
                            data1[x*3] = saturate(t);
                            t = (Y + Cb*g_cb + Cr*g_cr) >> (fixc + 2);
                            data1[x*3 + 1] = saturate(t);
                            t = (Y + Cr*r_cr) >> (fixc + 2);
                            data1[x*3 + 2] = saturate(t);
                            Y = (cmp[x+1] + 128*4) << fixc;
                            t = (Y + Cb*b_cb) >> (fixc + 2);
                            data1[x*3 + 3] = saturate(t);
                            t = (Y + Cb*g_cb + Cr*g_cr) >> (fixc + 2);
                            data1[x*3 + 4] = saturate(t);
                            t = (Y + Cr*r_cr) >> (fixc + 2);
                            data1[x*3 + 5] = saturate(t);
                        }
                    }
                    for( ; x < x2; x++ )
                    {
                        int Y  = (cmp[x] + 128*4) << fixc;
                        int Cb = cmpCb[x >> x_shift];
                        int Cr = cmpCr[x >> x_shift];
                        int t = (Y + Cb*b_cb) >> (fixc + 2);
                        data1[x*3] = saturate(t);
                        t = (Y + Cb*g_cb + Cr*g_cr) >> (fixc + 2);
                        data1[x*3 + 1] = saturate(t);
                        t = (Y + Cr*r_cr) >> (fixc + 2);
                        data1[x*3 + 2] = saturate(t);
                    }
                }
                else
                {
                    for( x = 0; x < x2; x++ )
                    {
                        int val = descale( cmp[x] + 128*4, 2 );
                        data1[x] = saturate(val);
                    }
                }
            }
        }
        
        x1 += h[0];
        if( x1 >= m_width )
        {
            x1 = 0;
            y1 += v[0];
            data += v[0]*step;
            if( y1 >= m_height ) break;
        }
    }
}


void  GrFmtJpegReader::GetBlock( int* block, int c )
{
    memset( block, 0, 64*sizeof(block[0]) );
    
    assert( 0 <= c && c < 3 );
    const short* td = m_td[m_ci[c].td];
    const short* ta = m_ta[m_ci[c].ta];
    const int* tq = m_tq[m_ci[c].tq];
    
    // Get DC coefficient
    int i = 0, cat  = m_strm.GetHuff( td );
    int mask = bs_bit_mask[cat];
    int val  = m_strm.Get( cat );
    
    val -= (val*2 <= mask ? mask : 0);
    m_ci[c].dc_pred = val += m_ci[c].dc_pred;
    
    block[0] = descale(val * tq[0],16);
    
    // Get AC coeffs
    for(;;)
    {
        cat = m_strm.GetHuff( ta );
        if( cat == 0 ) break; // end of block
        
        i += (cat >> 4) + 1;
        cat &= 15;
        mask = bs_bit_mask[cat];
        val  = m_strm.Get( cat );
        cat  = zigzag[i];
        val -= (val*2 <= mask ? mask : 0);
        block[cat] = descale(val * tq[cat], 16);
        assert( i <= 63 );
        if( i >= 63 ) break;
    }
}



////////////////////// WJpegStream ///////////////////////

class WJpegBitStream : public WMBitStream
{
public:
    WMByteStream  m_low_strm;

    WJpegBitStream();
    ~WJpegBitStream();

    virtual void  Flush();
    virtual bool  Open( const char* filename );
    virtual void  Close();
    virtual void  WriteBlock();
    
protected:
};

WJpegBitStream::WJpegBitStream()
{
}


WJpegBitStream::~WJpegBitStream()
{
    Close();
    m_is_opened = false;
}



bool  WJpegBitStream::Open( const char* filename )
{
    Close();
    Allocate();
    
    m_is_opened = m_low_strm.Open( filename );
    if( m_is_opened )
    {
        m_block_pos = 0;
        ResetBuffer();
    }
    return m_is_opened;
}


void  WJpegBitStream::Close()
{
    if( m_is_opened )
    {
        Flush();
        m_low_strm.Close();
        m_is_opened = false;
    }
}


void  WJpegBitStream::Flush()
{
    Put( -1, m_bit_idx & 31 );
    *((unsigned*&)m_current)++ = m_val;
    WriteBlock();
    ResetBuffer();
}


void  WJpegBitStream::WriteBlock()
{
    uchar* ptr = m_start;
    if( !bsIsBigEndian() )
        bsBSwapBlock( m_start, m_current );

    while( ptr < m_current )
    {
        int val = *ptr++;
        m_low_strm.PutByte( val );
        if( val == 0xff )
        {
            m_low_strm.PutByte( 0 );
        }
    }
    
    m_current = m_start;
}


/////////////////////// GrFmtJpegWriter ///////////////////

class GrFmtJpegWriter
{
public:

    GrFmtJpegWriter( const char* _filename );
    ~GrFmtJpegWriter();

    bool  WriteImage( const uchar* data, int step,
                     int width, int height, int depth, int channels );

protected:

    std::string m_filename;
    WJpegBitStream  m_strm;
};


GrFmtJpegWriter::GrFmtJpegWriter( const char* _filename )
{
    m_filename = _filename;
}

GrFmtJpegWriter::~GrFmtJpegWriter()
{
}


// FDCT with postscaling
static void aan_fdct8x8( int *src, int *dst,
                         int step, const int *postscale )
{
    int  workspace[64], *work = workspace;
    int  i;

    // Pass 1: process rows
    for( i = 8; i > 0; i--, src += step, work += 8 )
    {
        int x0 = src[0], x1 = src[7];
        int x2 = src[3], x3 = src[4];

        int x4 = x0 + x1; x0 -= x1;
        x1 = x2 + x3; x2 -= x3;
    
        work[7] = x0; work[1] = x2;
        x2 = x4 + x1; x4 -= x1;

        x0 = src[1]; x3 = src[6];
        x1 = x0 + x3; x0 -= x3;
        work[5] = x0;

        x0 = src[2]; x3 = src[5];
        work[3] = x0 - x3; x0 += x3;

        x3 = x0 + x1; x0 -= x1;
        x1 = x2 + x3; x2 -= x3;

        work[0] = x1; work[4] = x2;

        x0 = descale((x0 - x4)*C0_707, fixb);
        x1 = x4 + x0; x4 -= x0;
        work[2] = x4; work[6] = x1;

        x0 = work[1]; x1 = work[3];
        x2 = work[5]; x3 = work[7];

        x0 += x1; x1 += x2; x2 += x3;
        x1 = descale(x1*C0_707, fixb);

        x4 = x1 + x3; x3 -= x1;
        x1 = (x0 - x2)*C0_382;
        x0 = descale(x0*C0_541 + x1, fixb);
        x2 = descale(x2*C1_306 + x1, fixb);

        x1 = x0 + x3; x3 -= x0;
        x0 = x4 + x2; x4 -= x2;

        work[5] = x1; work[1] = x0;
        work[7] = x4; work[3] = x3;
    }

    work = workspace;
    // pass 2: process columns
    for( i = 8; i > 0; i--, work++, postscale += 8, dst += 8 )
    {
        int  x0 = work[8*0], x1 = work[8*7];
        int  x2 = work[8*3], x3 = work[8*4];

        int  x4 = x0 + x1; x0 -= x1;
        x1 = x2 + x3; x2 -= x3;
    
        work[8*7] = x0; work[8*0] = x2;
        x2 = x4 + x1; x4 -= x1;

        x0 = work[8*1]; x3 = work[8*6];
        x1 = x0 + x3; x0 -= x3;
        work[8*4] = x0;

        x0 = work[8*2]; x3 = work[8*5];
        work[8*3] = x0 - x3; x0 += x3;

        x3 = x0 + x1; x0 -= x1;
        x1 = x2 + x3; x2 -= x3;

        dst[0] = descale(x1*postscale[0], postshift);
        dst[4] = descale(x2*postscale[4], postshift);

        x0 = descale((x0 - x4)*C0_707, fixb);
        x1 = x4 + x0; x4 -= x0;

        dst[2] = descale(x4*postscale[2], postshift);
        dst[6] = descale(x1*postscale[6], postshift);

        x0 = work[8*0]; x1 = work[8*3];
        x2 = work[8*4]; x3 = work[8*7];

        x0 += x1; x1 += x2; x2 += x3;
        x1 = descale(x1*C0_707, fixb);

        x4 = x1 + x3; x3 -= x1;
        x1 = (x0 - x2)*C0_382;
        x0 = descale(x0*C0_541 + x1, fixb);
        x2 = descale(x2*C1_306 + x1, fixb);

        x1 = x0 + x3; x3 -= x0;
        x0 = x4 + x2; x4 -= x2;

        dst[5] = descale(x1*postscale[5], postshift);
        dst[1] = descale(x0*postscale[1], postshift);
        dst[7] = descale(x4*postscale[7], postshift);
        dst[3] = descale(x3*postscale[3], postshift);
    }
}


bool  GrFmtJpegWriter::WriteImage( const uchar* data, int step,
                                   int width, int height, int /*depth*/, int _channels )
{
    assert( data && width > 0 && height > 0 );
    
    if( !m_strm.Open( m_filename.c_str() ) ) return false;

    // encode the header and tables
    // for each mcu:
    //   convert rgb to yuv with downsampling (if color).
    //   for every block:
    //     calc dct and quantize
    //     encode block.
    int x, y;
    int i, j;
    const int max_quality = 12;
    int   quality = max_quality;
    WMByteStream& lowstrm = m_strm.m_low_strm;
    int   fdct_qtab[2][64];
    unsigned huff_dc_tab[2][16];
    unsigned huff_ac_tab[2][256];
    int  channels = _channels > 1 ? 3 : 1;
    int  x_scale = channels > 1 ? 2 : 1, y_scale = x_scale;
    int  dc_pred[] = { 0, 0, 0 };
    int  x_step = x_scale * 8;
    int  y_step = y_scale * 8;
    int  block[6][64];
    int  buffer[1024];
    int  luma_count = x_scale*y_scale;
    int  block_count = luma_count + channels - 1;
    int  Y_step = x_scale*8;
    const int UV_step = 16;
    double inv_quality;

    if( quality < 3 ) quality = 3;
    if( quality > max_quality ) quality = max_quality;

    inv_quality = 1./quality;

    // Encode header
    lowstrm.PutBytes( jpegHeader, sizeof(jpegHeader) - 1 );
    
    // Encode quantization tables
    for( i = 0; i < (channels > 1 ? 2 : 1); i++ )
    {
        const uchar* qtable = i == 0 ? jpegTableK1_T : jpegTableK2_T;
        int chroma_scale = i > 0 ? luma_count : 1;
        
        lowstrm.PutWord( 0xffdb );   // DQT marker
        lowstrm.PutWord( 2 + 65*1 ); // put single qtable
        lowstrm.PutByte( 0*16 + i ); // 8-bit table

        // put coefficients
        for( j = 0; j < 64; j++ )
        {
            int idx = zigzag[j];
            int qval = cvRound(qtable[idx]*inv_quality);
            if( qval < 1 )
                qval = 1;
            if( qval > 255 )
                qval = 255;
            fdct_qtab[i][idx] = cvRound((1 << (postshift + 9))/
                                      (qval*chroma_scale*idct_prescale[idx]));
            lowstrm.PutByte( qval );
        }
    }

    // Encode huffman tables
    for( i = 0; i < (channels > 1 ? 4 : 2); i++ )
    {
        const uchar* htable = i == 0 ? jpegTableK3 : i == 1 ? jpegTableK5 :
                              i == 2 ? jpegTableK4 : jpegTableK6;
        int is_ac_tab = i & 1;
        int idx = i >= 2;
        int tableSize = 16 + (is_ac_tab ? 162 : 12);

        lowstrm.PutWord( 0xFFC4   );      // DHT marker
        lowstrm.PutWord( 3 + tableSize ); // define one huffman table
        lowstrm.PutByte( is_ac_tab*16 + idx ); // put DC/AC flag and table index
        lowstrm.PutBytes( htable, tableSize ); // put table

        bsCreateEncodeHuffmanTable( bsCreateSourceHuffmanTable(
            htable, buffer, 16, 9 ), is_ac_tab ? huff_ac_tab[idx] :
            huff_dc_tab[idx], is_ac_tab ? 256 : 16 );
    }

    // put frame header
    lowstrm.PutWord( 0xFFC0 );          // SOF0 marker
    lowstrm.PutWord( 8 + 3*channels );  // length of frame header
    lowstrm.PutByte( 8 );               // sample precision
    lowstrm.PutWord( height );
    lowstrm.PutWord( width );
    lowstrm.PutByte( channels );        // number of components

    for( i = 0; i < channels; i++ )
    {
        lowstrm.PutByte( i + 1 );  // (i+1)-th component id (Y,U or V)
        if( i == 0 )
            lowstrm.PutByte(x_scale*16 + y_scale); // chroma scale factors
        else
            lowstrm.PutByte(1*16 + 1);
        lowstrm.PutByte( i > 0 ); // quantization table idx
    }

    // put scan header
    lowstrm.PutWord( 0xFFDA );          // SOS marker
    lowstrm.PutWord( 6 + 2*channels );  // length of scan header
    lowstrm.PutByte( channels );        // number of components in the scan

    for( i = 0; i < channels; i++ )
    {
        lowstrm.PutByte( i+1 );             // component id
        lowstrm.PutByte( (i>0)*16 + (i>0) );// selection of DC & AC tables
    }

    lowstrm.PutWord(0*256 + 63);// start and end of spectral selection - for
                                // sequental DCT start is 0 and end is 63

    lowstrm.PutByte( 0 );  // successive approximation bit position 
                           // high & low - (0,0) for sequental DCT
    lowstrm.WriteBlock();

    // encode data
    for( y = 0; y < height; y += y_step, data += y_step*step )
    {
        for( x = 0; x < width; x += x_step )
        {
            int x_limit = x_step;
            int y_limit = y_step;
            const uchar* rgb_data = data + x*_channels;
            int* Y_data = block[0];

            if( x + x_limit > width ) x_limit = width - x;
            if( y + y_limit > height ) y_limit = height - y;

            memset( block, 0, block_count*64*sizeof(block[0][0]));
            
            if( channels > 1 )
            {
                int* UV_data = block[luma_count];

                for( i = 0; i < y_limit; i++, rgb_data += step, Y_data += Y_step )
                {
                    for( j = 0; j < x_limit; j++, rgb_data += _channels )
                    {
                        int r = rgb_data[2];
                        int g = rgb_data[1];
                        int b = rgb_data[0];

                        int Y = descale( r*y_r + g*y_g + b*y_b, fixc - 2) - 128*4;
                        int U = descale( r*cb_r + g*cb_g + b*cb_b, fixc - 2 );
                        int V = descale( r*cr_r + g*cr_g + b*cr_b, fixc - 2 );
                        int j2 = j >> (x_scale - 1); 

                        Y_data[j] = Y;
                        UV_data[j2] += U;
                        UV_data[j2 + 8] += V;
                    }

                    rgb_data -= x_limit*_channels;
                    if( ((i+1) & (y_scale - 1)) == 0 )
                    {
                        UV_data += UV_step;
                    }
                }
            }
            else
            {
                for( i = 0; i < y_limit; i++, rgb_data += step, Y_data += Y_step )
                {
                    for( j = 0; j < x_limit; j++ )
                        Y_data[j] = rgb_data[j]*4 - 128*4;
                }
            }

            for( i = 0; i < block_count; i++ )
            {
                int is_chroma = i >= luma_count;
                int src_step = x_scale * 8;
                int run = 0, val;
                int* src_ptr = block[i & -2] + (i & 1)*8;
                const unsigned* htable = huff_ac_tab[is_chroma];

                aan_fdct8x8( src_ptr, buffer, src_step, fdct_qtab[is_chroma] );

                j = is_chroma + (i > luma_count);
                val = buffer[0] - dc_pred[j];
                dc_pred[j] = buffer[0];

                {
                float a = (float)val;
                int cat = (((int&)a >> 23) & 255) - (126 & (val ? -1 : 0));

                assert( cat <= 11 );
                m_strm.PutHuff( cat, huff_dc_tab[is_chroma] );
                m_strm.Put( val - (val < 0 ? 1 : 0), cat );
                }

                for( j = 1; j < 64; j++ )
                {
                    val = buffer[zigzag[j]];

                    if( val == 0 )
                    {
                        run++;
                    }
                    else
                    {
                        while( run >= 16 )
                        {
                            m_strm.PutHuff( 0xF0, htable ); // encode 16 zeros
                            run -= 16;
                        }

                        {
                        float a = (float)val;
                        int cat = (((int&)a >> 23) & 255) - (126 & (val ? -1 : 0));

                        assert( cat <= 10 );
                        m_strm.PutHuff( cat + run*16, htable );
                        m_strm.Put( val - (val < 0 ? 1 : 0), cat );
                        }

                        run = 0;
                    }
                }

                if( run )
                {
                    m_strm.PutHuff( 0x00, htable ); // encode EOB
                }
            }
        }
    }

    // Flush 
    m_strm.Flush();

    lowstrm.PutWord( 0xFFD9 ); // EOI marker
    m_strm.Close();

    return true;
}

void writeJpeg(const std::string& filename, const Mat& img)
{
    GrFmtJpegWriter writer(filename.c_str());
    writer.WriteImage(img.data, (int)img.step, img.cols, img.rows, 0, img.channels());
}

Mat readJpeg(const std::string& filename)
{
    GrFmtJpegReader reader(filename.c_str());
    bool ok = reader.ReadHeader();
    if(!ok)
        return Mat();
    Mat img(reader.m_height, reader.m_width, CV_8UC3);
    reader.ReadData(img.data, (int)img.step, true);
    return img;
}

}
}



