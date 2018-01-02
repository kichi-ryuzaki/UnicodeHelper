/// @file   text/unicodeHelper/unicodeHelper.h
/// @brief  Unicodeのよく使うもろもろ
#ifndef             TEXT_UNICODE_HELPER_H___
#define             TEXT_UNICODE_HELPER_H___

#include <stdint.h>
#include <string>

/// @enum   unicodeHelperEncoding
/// @brief  エンコーディング
typedef enum {
    unicodeHelperEncoding_unknown   =  (0),
    unicodeHelperEncoding_utf8      =  (1),     //  utf-8
    unicodeHelperEncoding_utf16arch =  (2),     //  utf-16(実行中のcpuに添ったエンディアン)
    unicodeHelperEncoding_utf16le   =  (3),     //  utf-16(little endian)
    unicodeHelperEncoding_utf16be   =  (4),     //  utf-16(big endian)
    unicodeHelperEncoding_cp932     =  (5),     //  cp932
} unicodeHelperEncoding;

#if         defined(__cplusplus)
#define UNICODEHELPER_EXTERN_C  extern "C"
#else   //  defined(__cplusplus)
#define UNICODEHELPER_EXTERN_C
#endif  //  defined(__cplusplus)


/// @def    unicodeHelperReadByteStream
/// @brief  1[byte]を読み込む関数の型
/// @param  out_dst 読み込んだデータの出力先
/// @param  io_arg  ユーザー定義のパラメータ
/// @retval 0   読み込み出来ない
/// @retval その他  読み込み成功
UNICODEHELPER_EXTERN_C typedef  signed int(*unicodeHelperReadByteStream)( uint8_t*const out_dst,
                                                                          void*const    io_arg);

/// @def    unicodeHelperWriteByteStream
/// @brief  1[byte]を書き出す関数の型
/// @param  in_src  出力するデータ
/// @param  io_arg  ユーザー定義のパラメータ
/// @retval 0   出力出来ない
/// @retval その他  書き出し成功
UNICODEHELPER_EXTERN_C typedef  signed int(*unicodeHelperWriteByteStream)( uint8_t const    in_src,
                                                                           void*const       io_arg);

/// @fn unicodeHelperAnalyzeEncode
/// @brief  指定の文字列のエンコードが何かを調べる
/// @param  in_rstrm    入力用の関数
/// @param  io_arg  入出力関数に渡すユーザーパラメータ
/// @return エンコーディング
UNICODEHELPER_EXTERN_C unicodeHelperEncoding    unicodeHelperAnalyzeEncoding( unicodeHelperReadByteStream const in_rstrm,
                                                                              void*const                        io_arg);

/// @fn unicodeHelperConvert
/// @brief  エンコード変更
/// @param  in_wstrm    出力用の関数
/// @param  in_ecDst    出力先エンコード
/// @param  in_withBOM  BOMを出力
/// @param  in_rstrm    入力用の関数
/// @param  in_ecSrc    入力元エンコード
/// @param  io_arg  入出力関数に渡すユーザーパラメータ
/// @retval 0   全部は出力出来なかった
/// @retval その他  全部出力出来た
/// @attention  一番多そうな運用としては、二回の呼び出しにして、
/// 一回目はin_wstrmに出力サイズだけをカウントアップする関数を渡して
/// サイズ計測。
/// 二回目は実際に出力する関数を渡して出力っていうパターンかと。
UNICODEHELPER_EXTERN_C signed int   unicodeHelperConvert( unicodeHelperWriteByteStream const    in_wstrm,
                                                          unicodeHelperEncoding const           in_ecDst,
                                                          signed int const                      in_withBOM,
                                                          unicodeHelperReadByteStream const     in_rstrm,
                                                          unicodeHelperEncoding const           in_ecSrc,
                                                          void*const                            io_arg);

#endif  //  ndef    TEXT_UNICODE_HELPER_H___
//  End of Source [text/unicodeHelper/unicodeHelper.h]
