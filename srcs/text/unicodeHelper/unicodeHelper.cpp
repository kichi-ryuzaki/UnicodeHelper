/// @file   text/unicodeHelper/unicodeHelper.cpp
/// @brief  Unicodeのよく使うもろもろ
#include "unicodeHelper.h"
#include "text/unicodeHelper/unicodeHelperConfig.h"

#if         defined(UNICODE_HELPER_USE_CP932)
typedef struct {
    uint16_t                    source;
    uint16_t                    destination;
} s2d;

#include "cp932.inc"

//  テーブル内にあるsourceと一致するdestinationを返す
static uint16_t unicodeHelper_search( s2d const*const   in_table,
                                      uint32_t const    in_num,
                                      uint16_t const    in_source,
                                      uint16_t const    in_unHitDest)
{
    s2d const*                  pTableTop( in_table);
    uint32_t                    numCur( in_num);
    static uint32_t const       bsGran= 10UL;

    for(;;)
    {
        if( numCur< bsGran)
        {
            s2d const*const             tableTerm( pTableTop+ bsGran);
            for( s2d const* pCur= pTableTop; pCur!= tableTerm; pCur++)
            {
                uint16_t const              srcCur( pCur->source);

                if( srcCur< in_source)  break;
                if( srcCur> in_source)  continue;

                return  pCur->destination;
            }
            break;
        } else {
            uint32_t                    idxMdl( static_cast<uint32_t>( numCur>> 1));
            s2d const*const             pCur( static_cast<s2d const*>( pTableTop+ idxMdl));
            uint16_t const              srcCur( pCur->source);

            if( srcCur< in_source)
            {
                pTableTop                       = pCur+ 1;
                numCur                          = static_cast<uint32_t>( numCur- static_cast<uint32_t>( idxMdl+ 1UL));
            } else if( srcCur> in_source)
            {
                numCur                          = idxMdl;
            } else {
                return  pCur->destination;
            }
        }
    }
    return  in_unHitDest;
}
#endif  //  defined(UNICODE_HELPER_USE_CP932)

//  バッファリングする最大サイズ([byte])
static int const                sizeBufferedMax= 6; //  現状最大はu32ギリギリ使ったUTF-8の6[byte]

//  読み込み処理用
typedef struct {
    //  読み込み関数
    unicodeHelperReadByteStream _rStream;
    void*                       _arg;
    //  0:_rStreamはまだ読めるかもしれない -1:既に読めなくなっている
    signed int                  _EOS;
    //  現在のバッファの先頭にあたるデータ中のインデックス
    uint32_t                    _indexStream;
    //  バッファリングしてあるサイズ([byte])
    uint32_t                    _szBuffered;
    //  バッファの先頭位置
    uint32_t                    _indexBuffers;
    //  バッファリング内容
    uint8_t                     _buffer[sizeBufferedMax];
} readStream;

//  readStreamを初期化
static readStream*  unicodeHelper_readStreamClear( readStream*const                     io_target,
                                                   unicodeHelperReadByteStream const    in_rStream,
                                                   void*const                           io_arg)
{
    io_target->_rStream             = in_rStream;
    io_target->_arg                 = io_arg;
    io_target->_EOS                 = 0;
    io_target->_indexStream         = 0UL;
    io_target->_szBuffered          = 0UL;
    io_target->_indexBuffers        = 0UL;

    return  io_target;
}

//  1[byte]指定のインデックスから読み込み
static signed int   unicodeHelper_loadByte( uint8_t*const       out_dst,
                                            readStream*const    io_target,
                                            uint32_t const      in_idx)
{
    uint32_t const              idxInBuffered= (uint32_t)( in_idx- io_target->_indexStream);
    if( io_target->_indexStream<= in_idx
        && sizeBufferedMax> idxInBuffered)
    {
        while( io_target->_szBuffered<= idxInBuffered)
        {
            uint32_t const              idxRead= (uint32_t)( (uint32_t)( io_target->_indexBuffers
                                                                         + io_target->_szBuffered)
                                                             % sizeBufferedMax);
            if( io_target->_rStream( &io_target->_buffer[ idxRead], io_target->_arg)!= 0)
            {
                io_target->_szBuffered          += 1UL;
            } else {
                return  0;
            }
        }

        *out_dst                        = io_target->_buffer[ idxInBuffered];
        return  -1;
    }

    *out_dst                        = 0U;
    return  0;
}

//  指定のインデックスより手前でバッファリングしているものを破棄
static void unicodeHelper_releaseBuffer( readStream*const   io_target,
                                         uint32_t const     in_idx)
{
    if( io_target->_indexStream<= in_idx)
    {
        uint32_t const              szRelease= (uint32_t)( in_idx- io_target->_indexStream);
        io_target->_indexStream         = in_idx;
        if( szRelease< io_target->_szBuffered)
        {
            io_target->_szBuffered          = (uint32_t)( io_target->_szBuffered- szRelease);
            io_target->_indexBuffers        = (uint32_t)( (uint32_t)( io_target->_indexBuffers
                                                                      + szRelease)
                                                          % sizeBufferedMax);
        } else {
            io_target->_szBuffered          = 0UL;
            io_target->_indexBuffers            = 0UL;
        }
    }
}

//  アーキテクチャ依存のエンディアンでuint16_tを読み込む
static signed int   unicodeHelper_loadWordArch( uint16_t*const      out_dst,
                                                readStream*const    io_target,
                                                uint32_t const      in_idx)
{
    uint16_t                    result;

    if( unicodeHelper_loadByte( (uint8_t*)( (void*)( &result)), io_target, in_idx)!= 0)
    {
        if( unicodeHelper_loadByte( (uint8_t*)( (uint8_t*)( (void*)( &result))+ 1), io_target, (uint32_t)( in_idx+ 1))!= 0)
        {
            *out_dst                        = result;
            return  -1;
        }
    }
    *out_dst                        = 0;
    return  0;
}

//  リトルエンディアンでuint16_tを読み込む
static signed int   unicodeHelper_loadWordLE( uint16_t*const    out_dst,
                                              readStream*const  io_target,
                                              uint32_t const    in_idx)
{
    uint8_t                     uc1st;

    if( unicodeHelper_loadByte( &uc1st, io_target, in_idx)!= 0)
    {
        uint8_t                     uc2nd;
        if( unicodeHelper_loadByte( &uc2nd, io_target, (uint32_t)( in_idx+ 1))!= 0)
        {
            *out_dst                        = (uint16_t)( (uint16_t)( ( (uint16_t)uc2nd)<< 8)
                                                          | (uint16_t)uc1st);
            return  -1;
        }
    }
    return  0;
}

//  ビッグエンディアンでuint16_tを読み込む
static signed int   unicodeHelper_loadWordBE( uint16_t*const    out_dst,
                                              readStream*const  io_target,
                                              uint32_t const    in_idx)
{
    uint8_t                     uc1st;

    if( unicodeHelper_loadByte( &uc1st, io_target, in_idx)!= 0)
    {
        uint8_t                     uc2nd;
        if( unicodeHelper_loadByte( &uc2nd, io_target, (uint32_t)( in_idx+ 1))!= 0)
        {
            *out_dst                        = (uint16_t)( (uint16_t)( ( (uint16_t)uc1st)<< 8)
                                                          | (uint16_t)uc2nd);
            return  -1;
        }
    }
    return  0;
}

//  書き出し処理用
typedef struct {
    //  書き出し関数
    unicodeHelperWriteByteStream _wStream;
    void*                       _arg;
} writeStream;

//  writeStreamを初期化
static writeStream* unicodeHelper_writeStreamClear( writeStream*const                   io_target,
                                                    unicodeHelperWriteByteStream const  in_wStream,
                                                    void*const                          io_arg)
{
    io_target->_wStream             = in_wStream;
    io_target->_arg                 = io_arg;

    return  io_target;
}

//  1[byte]出力
static signed int   unicodeHelper_storeByte( writeStream*const  io_target,
                                             uint8_t const      in_tar)
{
    return  io_target->_wStream( in_tar, io_target->_arg);
}

//  アーキテクチャ依存のエンディアンでuint16_tを書き込む
static signed int   unicodeHelper_storeWordArch( writeStream*const  io_target,
                                                 uint16_t const     in_tar)
{
    if( unicodeHelper_storeByte( io_target, *(uint8_t const*)( (void const*)&in_tar))!= 0
        && unicodeHelper_storeByte( io_target, *(uint8_t const*)( (uint8_t const*)( (void const*)&in_tar)+ 1))!= 0)
    {
        return  -1;
    }
    return  0;
}

//  リトルエンディアンでuint16_tを書き込む
static signed int   unicodeHelper_storeWordLE( writeStream*const    io_target,
                                               uint16_t const       in_tar)
{
    if( unicodeHelper_storeByte( io_target, (uint8_t)( in_tar& 0x00ffU))!= 0
        && unicodeHelper_storeByte( io_target, (uint8_t)( in_tar>> 8))!= 0)
    {
        return  -1;
    }
    return  0;
}

//  ビッグエンディアンでuint16_tを書き込む
static signed int   unicodeHelper_storeWordBE( writeStream*const    io_target,
                                               uint16_t const       in_tar)
{
    if( unicodeHelper_storeByte( io_target, (uint8_t)( in_tar>> 8))!= 0
        && unicodeHelper_storeByte( io_target, (uint8_t)( in_tar& 0x00ffU))!= 0)
    {
        return  -1;
    }
    return  0;
}





//  utf-8形式で一文字分入力
static signed int   unicodeHelper_loadUTF8( uint32_t*const      out_unicode,
                                            readStream*const    io_target,
                                            uint32_t* const     io_idx)
{
    uint8_t                     uc1st;
    uint32_t const              idxTop= *io_idx;
    if( unicodeHelper_loadByte( &uc1st, io_target, idxTop)!= 0)
    {
        if( uc1st< 0x80U)
        {
            *out_unicode                    = (uint32_t)uc1st;
            *io_idx                         = (uint32_t)( idxTop+ 1UL);
            return  -1;
        } else if( (uint8_t)( uc1st& 0xc0U)== 0xc0U)
        {
            uint8_t                     mask=   0xe0U;
            uint8_t                     prefix= 0xc0U;
            uint32_t                    valueLow= 0UL;
            for( int i= 1; i< 6; i++)
            {
                uint8_t                     ucCur;
                if( unicodeHelper_loadByte( &ucCur, io_target, (uint32_t)( idxTop+ i))!= 0)
                {
                    break;
                }
                if( (uint8_t)( ucCur& 0x80U)== 0)
                {
                    break;
                }
                valueLow                        = (uint32_t)( (uint32_t)( valueLow<< 6)+ (uint32_t)( ucCur& 0x6fU));
                if( (uint8_t)( uc1st& mask)== prefix)
                {
                    *out_unicode                    = (uint32_t)( (uint32_t)( (uint32_t)( ucCur
                                                                                          & (uint8_t)( ~mask))
                                                                              << (int)( i* 6))
                                                                  + valueLow);
                    *io_idx                         = (uint32_t)( idxTop+ i);
                    return  -1;
                }
                mask                            = (uint8_t)( (uint8_t)( mask>>   1)| 0x80U);
                prefix                          = (uint8_t)( (uint8_t)( prefix>> 1)| 0x80U);
            }
        }
    }

    *out_unicode                    = 0UL;

    return  0;
}

//  utf-8形式で指定のunicode値を出力
static signed int   unicodeHelper_storeUTF8( writeStream*const  io_target,
                                             uint32_t const     in_unicode)
{
    if( in_unicode< 0x00000080UL)
    {
        return  unicodeHelper_storeByte( io_target, (uint8_t)( in_unicode& 0x0000007fUL));
    } else if( in_unicode< 0x00000800UL)
    {
        uint8_t const               uc1st= (uint8_t)( (uint8_t)( (uint32_t)( in_unicode>>  6)& 0x0000001fUL)| 0xc0U);
        uint8_t const               uc2nd= (uint8_t)( (uint32_t)( in_unicode& 0x0000003fUL)| 0x80U);
        if( unicodeHelper_storeByte( io_target, uc1st)!= 0
            && unicodeHelper_storeByte( io_target, uc2nd)!= 0)
        {
            return  -1;
        }
    } else if( in_unicode< 0x00010000UL)
    {
        uint8_t const               uc1st= (uint8_t)( (uint8_t)( (uint32_t)( in_unicode>> 12)& 0x0000000fUL)| 0xe0U);
        uint8_t const               uc2nd= (uint8_t)( (uint32_t)( (uint32_t)( in_unicode>> 6)& 0x0000003fUL)| 0x80U);
        uint8_t const               uc3rd= (uint8_t)( (uint32_t)( in_unicode& 0x0000003fUL)| 0x80U);
        if( unicodeHelper_storeByte( io_target, uc1st)!= 0
            && unicodeHelper_storeByte( io_target, uc2nd)!= 0
            && unicodeHelper_storeByte( io_target, uc3rd)!= 0)
        {
            return  -1;
        }
    } else if( in_unicode< 0x00200000UL)
    {
        uint8_t const               uc1st= (uint8_t)( (uint8_t)( (uint32_t)( in_unicode>>  18)& 0x00000007UL)| 0xf0U);
        uint8_t const               uc2nd= (uint8_t)( (uint32_t)( (uint32_t)( in_unicode>> 12)& 0x0000003fUL)| 0x80U);
        uint8_t const               uc3rd= (uint8_t)( (uint32_t)( (uint32_t)( in_unicode>>  6)& 0x0000003fUL)| 0x80U);
        uint8_t const               uc4th= (uint8_t)( (uint32_t)( in_unicode& 0x0000003fUL)| 0x80U);
        if( unicodeHelper_storeByte( io_target, uc1st)!= 0
            && unicodeHelper_storeByte( io_target, uc2nd)!= 0
            && unicodeHelper_storeByte( io_target, uc3rd)!= 0
            && unicodeHelper_storeByte( io_target, uc4th)!= 0)
        {
            return  -1;
        }
    } else if( in_unicode< 0x04000000UL)
    {
        uint8_t const               uc1st= (uint8_t)( (uint8_t)( (uint32_t)( in_unicode>>  24)& 0x00000003UL)| 0xf8U);
        uint8_t const               uc2nd= (uint8_t)( (uint32_t)( (uint32_t)( in_unicode>> 18)& 0x0000003fUL)| 0x80U);
        uint8_t const               uc3rd= (uint8_t)( (uint32_t)( (uint32_t)( in_unicode>> 12)& 0x0000003fUL)| 0x80U);
        uint8_t const               uc4th= (uint8_t)( (uint32_t)( (uint32_t)( in_unicode>>  6)& 0x0000003fUL)| 0x80U);
        uint8_t const               uc5th= (uint8_t)( (uint32_t)( in_unicode& 0x0000003fUL)| 0x80U);
        if( unicodeHelper_storeByte( io_target, uc1st)!= 0
            && unicodeHelper_storeByte( io_target, uc2nd)!= 0
            && unicodeHelper_storeByte( io_target, uc3rd)!= 0
            && unicodeHelper_storeByte( io_target, uc4th)!= 0
            && unicodeHelper_storeByte( io_target, uc5th)!= 0)
        {
            return  -1;
        }
    } else if( in_unicode< 0x7fffffffUL)
    {
        uint8_t const               uc1st= (uint8_t)( (uint8_t)( (uint32_t)( in_unicode>>  30)& 0x00000001UL)| 0xfcU);
        uint8_t const               uc2nd= (uint8_t)( (uint32_t)( (uint32_t)( in_unicode>> 24)& 0x0000003fUL)| 0x80U);
        uint8_t const               uc3rd= (uint8_t)( (uint32_t)( (uint32_t)( in_unicode>> 18)& 0x0000003fUL)| 0x80U);
        uint8_t const               uc4th= (uint8_t)( (uint32_t)( (uint32_t)( in_unicode>> 12)& 0x0000003fUL)| 0x80U);
        uint8_t const               uc5th= (uint8_t)( (uint32_t)( (uint32_t)( in_unicode>>  6)& 0x0000003fUL)| 0x80U);
        uint8_t const               uc6th= (uint8_t)( (uint32_t)( in_unicode& 0x0000003fUL)| 0x80U);
        if( unicodeHelper_storeByte( io_target, uc1st)!= 0
            && unicodeHelper_storeByte( io_target, uc2nd)!= 0
            && unicodeHelper_storeByte( io_target, uc3rd)!= 0
            && unicodeHelper_storeByte( io_target, uc4th)!= 0
            && unicodeHelper_storeByte( io_target, uc5th)!= 0
            && unicodeHelper_storeByte( io_target, uc6th)!= 0)
        {
            return  -1;
        }
    }

    return  0;
}

//  utf-16コードの値
typedef struct {
    uint16_t                    uw1st;
    uint16_t                    uw2nd;
} utf16Pair;

//  サロゲートペアをunicode値に変更
static uint32_t unicodeHelper_FromSurrogatePair( utf16Pair const*const in_src)
{
    return  (uint32_t)( (uint32_t)( (uint32_t)( (uint16_t)( in_src->uw1st& 0x03ffU)+ 0x0040U)<< 10)
                        | (uint32_t)( in_src->uw2nd& 0x03ffU));
}

//  unicode値をサロゲートペアに変更
static signed int   unicodeHelper_ToSurrogatePair( utf16Pair*const  io_dst,
                                                   uint32_t const   in_unicode)
{
    if( in_unicode< 0x00010000UL)
    {
        io_dst->uw1st                   = (uint16_t)in_unicode;
        io_dst->uw2nd                   = 0UL;
        return  -1;
    } else if( in_unicode>= 0x00010000UL&& in_unicode<= 0x0010ffffUL)
    {
        io_dst->uw1st                   = (uint16_t)( (uint16_t)( (uint32_t)( (uint32_t)( in_unicode
                                                                                          - 0x0010000UL)
                                                                              & 0x000ffc00)>> 10)
                                                      | 0xd800U);
        io_dst->uw2nd                   = (uint16_t)( (uint16_t)( in_unicode& 0x000003ffUL)| 0xdc00U);
        return  -1;
    }
    return  0;
}


//  utf-16arch形式で一文字分入力
static signed int   unicodeHelper_loadUTF16Arch( uint32_t*const     out_unicode,
                                                 readStream*const   io_target,
                                                 uint32_t*const     io_idx)
{
    utf16Pair                   utf16;
    uint32_t const              idxTop= *io_idx;
    if( unicodeHelper_loadWordArch( &utf16.uw1st, io_target, idxTop)!= 0)
    {
        if( (uint16_t)( utf16.uw1st& 0xfc00)!= 0xd800U)
        {
            *out_unicode                    = (uint32_t)utf16.uw1st;
            return  -1;
        } else {
            if( unicodeHelper_loadWordArch( &utf16.uw2nd, io_target, (uint32_t)( idxTop+ 1UL))!= 0)
            {
                if( (uint16_t)( utf16.uw2nd& 0xfc00)!= 0xdc00U)
                {
                    *out_unicode                    = unicodeHelper_FromSurrogatePair( &utf16);
                    return  -1;
                }
            }
        }
    }
    return  0;
}

//  utf-16arch形式で指定のunicode値を出力
static signed int   unicodeHelper_storeUTF16Arch( writeStream*const io_target,
                                                  uint32_t const    in_unicode)
{
    utf16Pair                   utf16;
    if( unicodeHelper_ToSurrogatePair( &utf16, in_unicode)!= 0)
    {
        if( unicodeHelper_storeWordArch( io_target, utf16.uw1st)!= 0)
        {
            if( utf16.uw2nd== 0U
                || unicodeHelper_storeWordArch( io_target, utf16.uw2nd)!= 0)
            {
                return  -1;
            }
        }
    }

    return  0;
}

//  utf-16le形式で一文字分入力
static signed int   unicodeHelper_loadUTF16LE( uint32_t*const   out_unicode,
                                               readStream*const io_target,
                                               uint32_t*const   io_idx)
{
    utf16Pair                   utf16;
    uint32_t const              idxTop= *io_idx;
    if( unicodeHelper_loadWordLE( &utf16.uw1st, io_target, idxTop)!= 0)
    {
        if( (uint16_t)( utf16.uw1st& 0xfc00)!= 0xd800U)
        {
            *out_unicode                    = (uint32_t)utf16.uw1st;
            return  -1;
        } else {
            if( unicodeHelper_loadWordLE( &utf16.uw2nd, io_target, (uint32_t)( idxTop+ 1UL))!= 0)
            {
                if( (uint16_t)( utf16.uw2nd& 0xfc00)!= 0xdc00U)
                {
                    *out_unicode                    = unicodeHelper_FromSurrogatePair( &utf16);
                    return  -1;
                }
            }
        }
    }
    return  0;
}

//  utf-16le形式で指定のunicode値を出力
static signed int   unicodeHelper_storeUTF16LE( writeStream*const   io_target,
                                                uint32_t const      in_unicode)
{
    utf16Pair                   utf16;
    if( unicodeHelper_ToSurrogatePair( &utf16, in_unicode)!= 0)
    {
        if( unicodeHelper_storeWordLE( io_target, utf16.uw1st)!= 0)
        {
            if( utf16.uw2nd== 0U
                || unicodeHelper_storeWordLE( io_target, utf16.uw2nd)!= 0)
            {
                return  -1;
            }
        }
    }

    return  0;
}

//  utf-16be形式で一文字分入力
static signed int   unicodeHelper_loadUTF16BE( uint32_t*const   out_unicode,
                                               readStream*const io_target,
                                               uint32_t*const   io_idx)
{
    utf16Pair                   utf16;
    uint32_t const              idxTop= *io_idx;
    if( unicodeHelper_loadWordBE( &utf16.uw1st, io_target, idxTop)!= 0)
    {
        if( (uint16_t)( utf16.uw1st& 0xfc00)!= 0xd800U)
        {
            *out_unicode                    = (uint32_t)utf16.uw1st;
            return  -1;
        } else {
            if( unicodeHelper_loadWordBE( &utf16.uw2nd, io_target, (uint32_t)( idxTop+ 1UL))!= 0)
            {
                if( (uint16_t)( utf16.uw2nd& 0xfc00)!= 0xdc00U)
                {
                    *out_unicode                    = unicodeHelper_FromSurrogatePair( &utf16);
                    return  -1;
                }
            }
        }
    }
    return  0;
}

//  utf-16be形式で指定のunicode値を出力
static signed int   unicodeHelper_storeUTF16BE( writeStream*const   io_target,
                                                uint32_t const      in_unicode)
{
    utf16Pair                   utf16;
    if( unicodeHelper_ToSurrogatePair( &utf16, in_unicode)!= 0)
    {
        if( unicodeHelper_storeWordBE( io_target, utf16.uw1st)!= 0)
        {
            if( utf16.uw2nd== 0U
                || unicodeHelper_storeWordBE( io_target, utf16.uw2nd)!= 0)
            {
                return  -1;
            }
        }
    }

    return  0;
}

#if         defined(UNICODE_HELPER_USE_CP932)

//  cp932形式で一文字入力
static signed int   unicodeHelper_loadCP932( uint32_t*const     out_unicode,
                                             readStream*const   io_target,
                                             uint32_t*const     io_idx)
{
    uint8_t                     uc1st;
    uint32_t const              idxTop= *io_idx;
    if( unicodeHelper_loadByte( &uc1st, io_target, idxTop)!= 0)
    {
        if( uc1st< 0x80U)
        {
            if( uc1st!= 0U)
            {
                uint16_t const              unicode= unicodeHelper_search( &cp932_c2uc[ 0],
                                                                           (uint32_t)( sizeof(cp932_c2uc)/ sizeof(cp932_c2uc[0])),
                                                                           (uint16_t)uc1st,
                                                                           0x0000U);
                if( unicode!= 0U)
                {
                    *out_unicode                    = (uint32_t)unicode;
                    *io_idx                         = (uint32_t)( idxTop+ 1);
                    return  -1;
                }
            } else {
                *out_unicode                    = 0UL;
                *io_idx                         = (uint32_t)( idxTop+ 1);
                return  -1;
            }
        } else {
            uint8_t                     uc2nd;
            if( unicodeHelper_loadByte( &uc2nd, io_target, (uint32_t)( idxTop+ 1UL))!= 0)
            {
                uint16_t const              cp932= (uint16_t)( (uint16_t)( ( (uint16_t)uc1st)<< 8)
                                                               | (uint16_t)uc2nd);
                uint16_t const              unicode= unicodeHelper_search( &cp932_c2uc[ 0],
                                                                           (uint32_t)( sizeof(cp932_c2uc)/ sizeof(cp932_c2uc[0])),
                                                                           cp932,
                                                                           0x0000U);
                if( unicode!= 0U)
                {
                    *out_unicode                    = (uint32_t)unicode;
                    *io_idx                         = (uint32_t)( idxTop+ 1);
                    return  -1;
                }
            }
        }
    }

    *out_unicode                    = 0UL;
    return  0;
}

//  cp932形式で一文字出力
static signed int   unicodeHelper_storeCP932( writeStream*const io_target,
                                              uint32_t const    in_unicode)
{
    if( in_unicode< 0x00010000UL)
    {
        if( in_unicode!= 0UL)
        {
            uint16_t const              cp932= unicodeHelper_search( &cp932_uc2c[ 0],
                                                                     (uint32_t)( sizeof(cp932_uc2c)/ sizeof(cp932_uc2c[0])),
                                                                     (uint16_t)( in_unicode& 0x0000ffffUL),
                                                                     0x0000U);
            if( cp932!= 0U)
            {
                if( cp932& 0xff00U)
                {
                    if( unicodeHelper_storeByte( io_target, (uint8_t)( cp932>> 8))!= 0
                        && unicodeHelper_storeByte( io_target, (uint8_t)( cp932& 0x00ffU))!= 0)
                    {
                        return  -1;
                    }
                } else {
                    if( unicodeHelper_storeByte( io_target, (uint8_t)( cp932& 0x00ffU))!= 0)
                    {
                        return  -1;
                    }
                }
            }
        } else {
            if( unicodeHelper_storeByte( io_target, 0U)!= 0)
            {
                return  -1;
            }
        }
    }

    return  0;
}
#else   //  defined(UNICODE_HELPER_USE_CP932)

//  cp932をサポートしない場合の一文字読み込み用ダミー関数
static signed int   unicodeHelper_loadCP932( uint32_t*const,
                                             readStream*const,
                                             uint32_t*const)
{
    return  0;
}

//  cp932をサポートしない場合の一文字書き出し用ダミー関数
static signed int   unicodeHelper_storeCP932( writeStream*const, uint32_t const)
{
    return  0;
}

#endif  //  defined(UNICODE_HELPER_USE_CP932)

//  何かのエンコードで指定のreadStreamからunicodeを読み込む関数の型
typedef signed int(*loadFunc)( uint32_t*const   /*  unicodeの出力先  */,
                               readStream*const /*  読み込みストリーム */,
                               uint32_t*const   /*  入力:読み込み開始ofs 出力: 読み込み後のofs */);

//  何かのエンコードで指定のwriteStreamへunicodeを書き込む関数の型
typedef signed int(*storeFunc)( writeStream*const   /*  書き出しストリーム */,
                                uint32_t const      /*  unicode */ );


//  指定エンコーディングで1文字読み込む関数へのポインタ取得
static loadFunc unicodeHelperGetLoadFunc( unicodeHelperEncoding const in_target)
{
    switch( in_target)
    {
    case    unicodeHelperEncoding_utf8:         return  unicodeHelper_loadUTF8;
    case    unicodeHelperEncoding_utf16arch:    return  unicodeHelper_loadUTF16Arch;
    case    unicodeHelperEncoding_utf16le:      return  unicodeHelper_loadUTF16LE;
    case    unicodeHelperEncoding_utf16be:      return  unicodeHelper_loadUTF16BE;
    case    unicodeHelperEncoding_cp932:        return  unicodeHelper_loadCP932;
    }

    return  (loadFunc)0;
}

//  指定エンコーディングで1文字書き出す関数へのポインタ取得
static storeFunc    unicodeHelperGetStoreFunc( unicodeHelperEncoding const in_target)
{
    switch( in_target)
    {
    case    unicodeHelperEncoding_utf8:         return  unicodeHelper_storeUTF8;
    case    unicodeHelperEncoding_utf16arch:    return  unicodeHelper_storeUTF16Arch;
    case    unicodeHelperEncoding_utf16le:      return  unicodeHelper_storeUTF16LE;
    case    unicodeHelperEncoding_utf16be:      return  unicodeHelper_storeUTF16BE;
    case    unicodeHelperEncoding_cp932:        return  unicodeHelper_storeCP932;
    }

    return  (storeFunc)0;
}

//  BOMがあるようなら、readStreamをその分飛ばす
static void unicodeHelperSkipBOM( readStream*const  in_prs,
                                  loadFunc const    in_loadFunc)
{
    uint32_t                    unicode;

    if( in_loadFunc( &unicode, in_prs, &in_prs->_indexStream)!= 0)
    {
        if( unicode== 0x0000feffUL)
        {
            //  BOMなので除外
            unicodeHelper_releaseBuffer( in_prs, (uint32_t)( in_prs->_indexStream+ in_prs->_szBuffered));
        }
    }
}

//  BOMを出力する
static signed int   unicodeHelperStoreBOM( writeStream*     in_pws,
                                           storeFunc const  in_storeFunc)
{
    // BOM出力
    if( in_storeFunc( in_pws, 0x0000feffUL)== 0)
    {
        return  0;
    }

    return  -1;
}

//  エンコード解析のための1unicodehelperEncoding単位の調査用ワーク
typedef struct {
    signed int                  _isValid;       //  0:このエンコードは候補ではない -1:このエンコードは現在調査中
    uint32_t                    _index;         //  次に読み出すreadStreamのオフセット
    uint32_t                    _maxReadSize;   //  このエンコードの一文字の最大サイズ([byte])
    loadFunc                    _loadFunc;      //  一文字読み込み用の関数へのポインタ
} analyze;

//  analyze構造体を初期化
static analyze* unicodeHelper_analyzeClear( analyze*const   out_analyze,
                                            uint32_t const  in_maxReadSize,
                                            loadFunc const  in_loadFunc)
{
    out_analyze->_isValid           = -1;
    out_analyze->_index             = 0UL;
    out_analyze->_maxReadSize       = in_maxReadSize;
    out_analyze->_loadFunc          = in_loadFunc;
    return  out_analyze;
}

//  readStreamのバッファ内に次の文字が読めるだけの未読or格納領域があるか
static signed int   unicodeHelper_analyzeIsCheckTarget( analyze const*const     in_analyze,
                                                        readStream const*const  in_rStream)
{
    if( in_analyze->_isValid!= 0)
    {

        if( in_analyze->_index>= in_rStream->_indexBuffers)
        {

            uint32_t                    offsetInBuff= (uint32_t)( in_analyze->_index- in_rStream->_indexBuffers);
            if( (uint32_t)( offsetInBuff+ in_analyze->_maxReadSize)<= sizeBufferedMax)
            {
                return  -1;
            }
        }
    }
    return  0;
}

//  analyzeの指定エンコーディングで直近の一文字が読めたかどうか
static signed int   unicodeHelper_analyzeTest( analyze*const    io_analyze,
                                               readStream*const io_stream)
{
    uint32_t                    unicode;

    if( io_analyze->_loadFunc( &unicode, io_stream, &io_analyze->_index)!= 0)
    {
    } else {
        io_analyze->_isValid            = 0;
    }
    return  io_analyze->_isValid;
}

//  analyzeの初期化を列挙するための構造体
typedef struct {
    unicodeHelperEncoding       _encoding;      //  エンコード
    uint32_t                    _maxReadSize;   //  一文字の最大サイズ([byte])
} encodingAndLoadFunc;

//  unicodeHelperAnalyzeEncoding()中の、エンコード単位のanalyze初期化用パラメータ配列
static encodingAndLoadFunc const gAnalyzeTargetAry[]= {
    { unicodeHelperEncoding_utf8,    6UL},
    { unicodeHelperEncoding_utf16le, 4UL},
    { unicodeHelperEncoding_utf16be, 4UL},
    { unicodeHelperEncoding_cp932,   2UL}
};

//  エンコーディング読解
UNICODEHELPER_EXTERN_C unicodeHelperEncoding    unicodeHelperAnalyzeEncoding( unicodeHelperReadByteStream const in_rStrm,
                                                                              void*const                        io_arg)
{
    readStream                  rs;

    readStream*const            prs= unicodeHelper_readStreamClear( &rs, in_rStrm, io_arg);

    //  まずここで、BOMのチェックをします
    uint8_t                     uc1st, uc2nd;
    if( unicodeHelper_loadByte( &uc1st, prs, 0UL)!= 0
        && unicodeHelper_loadByte( &uc2nd, prs, 1UL)!= 0)
    {
        if( uc1st== 0xffU&& uc2nd== 0xfeU)  return  unicodeHelperEncoding_utf16le;
        if( uc1st== 0xfeU&& uc2nd== 0xffU)  return  unicodeHelperEncoding_utf16be;

        if( uc1st== 0xefU&& uc2nd== 0xbbU)
        {
            uint8_t                     uc3rd;
            if( unicodeHelper_loadByte( &uc3rd, prs, 2UL)!= 0
                && uc3rd== 0xbfU)
            {
                return  unicodeHelperEncoding_utf8;
            }
        }
    }
    {
        //  エンコーディング単位の解析情報を保存する構造体を初期化
        analyze                     analyzeAry[ sizeof(gAnalyzeTargetAry)/ sizeof(gAnalyzeTargetAry[0])];

        for( int i= 0; i< sizeof(analyzeAry)/ sizeof(analyzeAry[0]); i++)
        {
            unicodeHelper_analyzeClear( &analyzeAry[ i],
                                        gAnalyzeTargetAry[ i]._maxReadSize,
                                        unicodeHelperGetLoadFunc( gAnalyzeTargetAry[ i]._encoding));
        }


        //  全エンコーディングについて、読めない文字が出るまで読んでい
        //  く
        for(;;)
        {
            unicodeHelperEncoding       lastHitEncoding= unicodeHelperEncoding_unknown;
            int                         validEntryNum= 0;
            signed int                  idxCurMinIsValid= 0;
            uint32_t                    idxCurMin= 0UL;
            for( int i= 0; i< sizeof(analyzeAry)/ sizeof(analyzeAry[0]); i++)
            {
                analyze*const               analyzeCur= &analyzeAry[ i];
                //  既に除外されているエンコードは省略
                if( analyzeCur->_isValid== 0)   continue;

                //  prsの読み取り範囲の外に次の読み込み位置があるエンコー
                //  ドは保留
                if( unicodeHelper_analyzeIsCheckTarget( analyzeCur, prs)== 0)
                {
                    if( idxCurMinIsValid== 0|| idxCurMin> analyzeCur->_index)
                    {
                        idxCurMinIsValid                = -1;
                        idxCurMin                       = analyzeCur->_index;
                    }
                    validEntryNum++;
                    lastHitEncoding                 = gAnalyzeTargetAry[ i]._encoding;
                    continue;
                }

                //  prsからunicodeを読めるかテスト
                if( unicodeHelper_analyzeTest( analyzeCur, prs)!= 0)
                {
                    if( idxCurMinIsValid== 0|| idxCurMin> analyzeCur->_index)
                    {
                        idxCurMinIsValid                = -1;
                        idxCurMin                       = analyzeCur->_index;
                    }
                    validEntryNum++;
                    lastHitEncoding                 = gAnalyzeTargetAry[ i]._encoding;
                } else {
                    //  候補外
                    analyzeCur->_isValid            = 0;
                }
            }
            if( validEntryNum<= 1)
            {
                //  こいつが候補です
                return  lastHitEncoding;
            }
            unicodeHelper_releaseBuffer( prs, (uint32_t)( idxCurMin- prs->_indexBuffers));
        }
    }
    return  unicodeHelperEncoding_unknown;
}


UNICODEHELPER_EXTERN_C signed int   unicodeHelperConvert( unicodeHelperWriteByteStream const    in_wStrm,
                                                          unicodeHelperEncoding const           in_ecDst,
                                                          signed int const                      in_withBOM,
                                                          unicodeHelperReadByteStream const     in_rStrm,
                                                          unicodeHelperEncoding const           in_ecSrc,
                                                          void*const                            io_arg)
{
    readStream                  rs;
    readStream*                 prs;
    writeStream                 ws;
    writeStream*                pws;
    uint32_t                    unicode;
    storeFunc                   pStore;

    //  入力エンコーディングごとに読み込み用の関数を分ける
    loadFunc const              pLoad= unicodeHelperGetLoadFunc( in_ecSrc);
    if( pLoad== (loadFunc)0)    return  0;

    //  出力エンコーディングごとに書き出し用の関数を分ける
    pStore                          = unicodeHelperGetStoreFunc( in_ecDst);
    if( pStore== (storeFunc)0)  return  0;

    //  入出力ストリームを初期化
    prs                             = unicodeHelper_readStreamClear(  &rs, in_rStrm, io_arg);
    pws                             = unicodeHelper_writeStreamClear( &ws, in_wStrm, io_arg);

    //  BOMがあるったらスキップ
    unicodeHelperSkipBOM( prs, pLoad);

    //  BOMの出力が必要なら出力
    if( in_withBOM!= 0&& unicodeHelperStoreBOM( pws, pStore)== 0)   return  0;

    //  一文字ごとに処理
    for(;;)
    {
        //  読み込み
        if( pLoad( &unicode, prs, &prs->_indexStream)== 0)
        {
            //  入力が切れてる?
            if( prs->_EOS!= 0&& prs->_szBuffered== 0UL)
            {
                return  -1;
            }
            //  入力が終わらなかったね
            break;
        }
        unicodeHelper_releaseBuffer( prs, (uint32_t)( prs->_indexStream+ prs->_szBuffered));

        //  書き出し
        if( pStore( pws, unicode)== 0)
        {
            break;
        }
    }
    return  0;
}
//  End of Source [text/unicodeHelper/unicodeHelper.cpp]
