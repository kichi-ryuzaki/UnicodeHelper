/// @file   convunicodeorg.cpp
/// @brief  unicode.orgで配布されている.TXTをc用のテーブル化する
#include <stdint.h>
#include <algorithm>
#include <fstream>
#include <functional>
#include <iterator>
#include <iomanip>
#include <string>
#include <vector>

/// @class  c2uc
/// @brief  unicodeとそれに対応する文字コードの対を保存する
class   c2uc
{
public:
    uint16_t                    code;
    uint16_t                    unicode;

    c2uc( uint16_t const    in_code,
          uint16_t const    in_unicode)
    : code(    in_code)
    , unicode( in_unicode)
    {
    }
};

//  十六進数の一文字をパース(16は十六進数以外が渡された)
static uint16_t parseHexAlphabet( char const in_alp)
{
    switch( in_alp)
    {
    case    '0':    return   0U;
    case    '1':    return   1U;
    case    '2':    return   2U;
    case    '3':    return   3U;
    case    '4':    return   4U;
    case    '5':    return   5U;
    case    '6':    return   6U;
    case    '7':    return   7U;
    case    '8':    return   8U;
    case    '9':    return   9U;
    case    'A':    return  10U;
    case    'B':    return  11U;
    case    'C':    return  12U;
    case    'D':    return  13U;
    case    'E':    return  14U;
    case    'F':    return  15U;
    case    'a':    return  10U;
    case    'b':    return  11U;
    case    'c':    return  12U;
    case    'd':    return  13U;
    case    'e':    return  14U;
    case    'f':    return  15U;
    default:        return  16U;
    }
}

//  十六進数をパース
static bool parseHex( uint16_t*const    out_val,    //  結果
                      size_t*const      out_idx,    //  次のパース位置
                      char const*const  in_src,     //  ソース
                      size_t const      in_size,    //  ソースのサイズ([byte])
                      size_t const      in_idx)     //  パースをする位置
{
    if( static_cast<size_t>( in_idx+ 3)< in_size)
    {
        if( *static_cast<char const*>( in_src+ in_idx)== '0')
        {
            char const                  c2nd( *static_cast<char const*>( in_src+ static_cast<size_t>( in_idx+ 1)));
            if( c2nd== 'x'|| c2nd== 'X')
            {
                size_t                      idx( static_cast<size_t>( in_idx+ 2));
                uint16_t                    val( parseHexAlphabet( *static_cast<char const*>( in_src+ idx)));
                if( val< 16U)
                {
                    idx++;
                    while( idx< in_size)
                    {
                        uint16_t                    valCur( parseHexAlphabet( *static_cast<char const*>( in_src+ idx)));
                        if( valCur>= 16U)   break;
                        val                             = static_cast<uint16_t>( static_cast<uint16_t>( val<< 4)+ valCur);
                        idx++;
                    }
                    *out_val                        = val;
                    *out_idx                        = idx;

                    return  true;
                }
            }
        }
    }

    return  false;
}

//  空白をスキップ
static size_t   skipBlank( char const*const in_src, //  ソース
                           size_t const     in_size,//  ソースサイズ([byte])
                           size_t const     in_idx) //  パースをする位置
{
    size_t                      rsp( in_idx);
    for( ; rsp< in_size;)
    {
        char const                  cCur( *static_cast<char const*>( in_src+ rsp));
        if( cCur== ' '|| cCur== '\t')
        {
            rsp++;
        } else {
            break;
        }
    }

    return  rsp;
}

//  元ファイルを読み込み、ユニコード対 対応コードのvectorで返す
static bool readTable( std::vector<c2uc>*io_dst, char const*const in_pathIn)
{
    std::ifstream               fs( in_pathIn, std::ios::in);

    if( fs.bad()== false)
    {
        while( fs.eof()== false)
        {
            std::string                 lb;
            std::getline( fs, lb);

            uint16_t                    code;
            size_t const                szLine( lb.length());
            size_t                      idx;
            if( parseHex( &code, &idx, lb.c_str(), szLine, 0)!= false)
            {
                idx                             = skipBlank( lb.c_str(), szLine, idx);
                uint16_t                    unicode;
                if( parseHex( &unicode, &idx, lb.c_str(), szLine, idx)!= false)
                {
                    io_dst->push_back( c2uc( code, unicode));
                }
            }
        }

        fs.close();

        return  true;
    } else {
        fprintf( stderr, "%s can't read.\n", in_pathIn);
        return  false;
    }
}

//  ユニコード対 対応コードのvector(要対応コードによるソート)を.incとして出力
static bool writeTable( std::vector<c2uc>const& in_sortedSource, char const*const in_pathOut, char const*const in_label)
{
    std::vector<c2uc>           reverse;
    reverse.reserve( in_sortedSource.size());
    std::copy( in_sortedSource.begin(),
               in_sortedSource.end(),
               std::back_inserter( reverse));

    std::sort( reverse.begin(),
               reverse.end(),
               []( c2uc const&  in_l,
                   c2uc const&  in_r) {
                   if( in_l.unicode< in_r.unicode)  return  true;
                   if( in_l.unicode> in_r.unicode)  return  false;

                   return   static_cast<bool>( in_l.code< in_r.code);
               });

    std::fstream                fs( in_pathOut, std::ios::out);

    if( fs.bad()== false)
    {
        static uint32_t const       numOneline= 4UL;
        fs<< "static s2d const "<< std::string( in_label)<< "_c2uc[]= {"<< std::endl;
        uint32_t                    idx( 0UL);
        for( std::vector<c2uc>::const_iterator it= in_sortedSource.cbegin(); it!= in_sortedSource.cend(); it++)
        {
            if( static_cast<uint32_t>( idx% numOneline)== 0UL)  fs<< " ";
            fs<< " { 0x";
            fs<< std::hex<< std::setfill( '0')<< std::setw( 4)<< it->code;
            fs<< "U, 0x";
            fs<< std::hex<< std::setfill( '0')<< std::setw( 4)<< it->unicode;
            fs<< "U},";
            idx++;
            if( static_cast<uint32_t>( idx% numOneline)== 0UL)  fs<< std::endl;
        }
        if( static_cast<uint32_t>( idx% numOneline)!= 0UL)  fs<< std::endl;
        fs<< "};"<< std::endl;

        fs<< std::endl;

        fs<< "static s2d const "<< std::string( in_label)<< "_uc2c[]= {"<< std::endl;
        idx                             = 0UL;
        for( std::vector<c2uc>::const_iterator it= reverse.cbegin(); it!= reverse.cend(); it++)
        {
            if( static_cast<uint32_t>( idx% numOneline)== 0UL)  fs<< " ";
            fs<< " { 0x";
            fs<< std::hex<< std::setfill( '0')<< std::setw( 4)<< it->unicode;
            fs<< "U, 0x";
            fs<< std::hex<< std::setfill( '0')<< std::setw( 4)<< it->code;
            fs<< "U},";
            idx++;
            if( static_cast<uint32_t>( idx% numOneline)== 0UL)  fs<< std::endl;
        }
        if( static_cast<uint32_t>( idx% numOneline)!= 0UL)  fs<< std::endl;
        fs<< "};"<< std::endl;

        fs.close();

        return  true;
    } else {
        fprintf( stderr, "%s can't write.\n", in_pathOut);
        return  false;
    }

}

//  エントリ
static int  genTable( char const*const in_pathIn, char const*const in_pathOut, char const*const in_label)
{
    int                         resp( -1);
    std::vector<c2uc>           source;

    if( readTable( &source, in_pathIn)!= false)
    {
        std::sort( source.begin(),
                   source.end(),
                   []( c2uc const&  in_l,
                       c2uc const&  in_r) {
                       if( in_l.code< in_r.code)    return  true;
                       if( in_l.code> in_r.code)    return  false;

                       return   static_cast<bool>( in_l.unicode< in_r.unicode);
                   });

        if( writeTable( source, in_pathOut, in_label)!= false)
        {
            resp                            = 0;
        }
    }

    return  resp;
}


int main( int in_argC, char** in_argV)
{
    if( in_argC== 4)
    {
        return  genTable( *static_cast<char**>( in_argV+ 1),
                          *static_cast<char**>( in_argV+ 2),
                          *static_cast<char**>( in_argV+ 3));
    } else {
        fprintf( stderr, "%s [UNICODE.TXT] [variable label] [OUTPUT.inc]\n", *in_argV);
        return  0;
    }
}

//  End of Source [convunicodeorg.cpp]
