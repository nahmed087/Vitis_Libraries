/*
 * Copyright 2019 Xilinx, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 *
 * @file ofb.hpp
 * @brief header file for Output Feedback (OFB) block cipher mode of operation.
 * This file is part of XF Security Library.
 *
 * @detail Containing OFB mode with AES-128/192/256 and DES.
 * Loop-carried dependency is encforced by both encryption and decryption part of OFB mode.
 *
 */

#ifndef _XF_SECURITY_OFB_HPP_
#define _XF_SECURITY_OFB_HPP_

#include <ap_int.h>
#include <hls_stream.h>

#include "xf_security/aes.hpp"
#include "xf_security/des.hpp"

// for debug
#ifndef __SYNTHESIS__
#include <iostream>
#endif

namespace xf {
namespace security {
namespace details {

/**
 *
 * @brief aesOfbEncrypt is OFB encryption mode with AES single block cipher.
 *
 * The algorithm reference is : "Recommendation for Block Cipher Modes of Operation - Methods and Techniques"
 * The implementation is modified for better performance.
 *
 * @tparam _keyWidth The bit-width of the cipher key, which is 128, 192, or 256.
 *
 * @param plaintext Input block stream text to be encrypted, each block is 128 bits.
 * @param plaintext_e End flag of block stream plaintext, 1 bit.
 * @param cipherkey Input cipher key used in encryption, x bits for AES-x.
 * @param initialization_vector Initialization vector for the fisrt iteration of AES encrypition, 128 bits.
 * @param ciphertext Output encrypted block stream text, each block is 128 bits.
 * @param ciphertext_e End flag of block stream ciphertext, 1 bit.
 *
 */

template <unsigned int _keyWidth = 256>
void aesOfbEncrypt(
    // stream in
    hls::stream<ap_uint<128> >& plaintext,
    hls::stream<bool>& plaintext_e,
    // input cipherkey and initialization vector
    hls::stream<ap_uint<_keyWidth> >& cipherkey,
    hls::stream<ap_uint<128> >& initialization_vector,
    // stream out
    hls::stream<ap_uint<128> >& ciphertext,
    hls::stream<bool>& ciphertext_e) {
    // register cipherkey
    ap_uint<_keyWidth> key_r = cipherkey.read();
#ifndef __SYNTHESIS__
    std::cout << std::endl << "cipherkey = " << std::hex << key_r << std::endl;
#endif
    xf::security::aesEnc<_keyWidth> cipher;
    cipher.updateKey(key_r);
    // register IV
    ap_uint<128> IV = initialization_vector.read();
#ifndef __SYNTHESIS__
    std::cout << "initialization_vector = " << std::hex << IV << std::endl << std::endl;
#endif

    // intermediate registers to perform the encryption chain
    ap_uint<128> plaintext_r = 0;
    ap_uint<128> feedback_r = 0;
    ap_uint<128> input_block = 0;
    ap_uint<128> output_block = 0;
    ap_uint<128> ciphertext_r = 0;

    // set the initialization for ture
    bool initialization = true;

    bool e = plaintext_e.read();

encryption_ofb_loop:
    while (!e) {
#pragma HLS PIPELINE
        // read a block of plaintext, 128 bits
        plaintext_r = plaintext.read();
#ifndef __SYNTHESIS__
        std::cout << "plaintext    = " << std::hex << plaintext_r << std::endl;
#endif

        // calculate input_block
        if (initialization) { // first iteration, input_block is IV
            input_block = IV;
            initialization = false;
        } else { // after first iteration, input_blcok is output_block of last iteration
            input_block = feedback_r;
        }
#ifndef __SYNTHESIS__
        std::cout << "input_block  = " << std::hex << input_block << std::endl;
#endif

        // CIPH_k
        cipher.process(input_block, key_r, output_block);
// xf::security::internal::aesEncrypt<_keyWidth>(input_block, key_r, output_block);
#ifndef __SYNTHESIS__
        std::cout << "output_block = " << std::hex << output_block << std::endl;
#endif

        // feedback for the next iteration and get the ciphertext for current interation
        ciphertext_r = plaintext_r ^ output_block;
        feedback_r = output_block;
#ifndef __SYNTHESIS__
        std::cout << "feedback     = " << std::hex << feedback_r << std::endl;
        std::cout << "ciphertext   = " << std::hex << ciphertext_r << std::endl;
#endif

        // write out ciphertext
        ciphertext.write(ciphertext_r);
        ciphertext_e.write(0);

        e = plaintext_e.read();
    }

    ciphertext_e.write(1);

} // end aesOfbEncrypt

/**
 *
 * @brief aesOfbDecrypt is OFB decryption mode with AES single block cipher.
 *
 * The algorithm reference is : "Recommendation for Block Cipher Modes of Operation - Methods and Techniques"
 * The implementation is modified for better performance.
 *
 * @tparam _keyWidth The bit-width of the cipher key, which is 128, 192, or 256.
 *
 * @param ciphertext Input block stream to be decrypted, each block is 128 bits.
 * @param ciphertext_e End flag of block stream ciphertext, 1 bit.
 * @param cipherkey Input cipher key used in decryption, x bits for AES-x.
 * @param IV_strm Initialization vector for the fisrt iteration of AES decrypition, 128 bits.
 * @param plaintext Output decrypted block stream text, each block is 128 bits.
 * @param plaintext_e End flag of block stream plaintext, 1 bit.
 *
 */

template <unsigned int _keyWidth = 256>
void aesOfbDecrypt(
    // stream in
    hls::stream<ap_uint<128> >& ciphertext,
    hls::stream<bool>& ciphertext_e,
    // input cipherkey & initialization vector
    hls::stream<ap_uint<_keyWidth> >& cipherkey,
    hls::stream<ap_uint<128> >& IV_strm,
    // stream out
    hls::stream<ap_uint<128> >& plaintext,
    hls::stream<bool>& plaintext_e) {
    // register cipherkey
    ap_uint<_keyWidth> key_r = cipherkey.read();
#ifndef __SYNTHESIS__
    std::cout << std::endl << "cipherkey = " << std::hex << key_r << std::endl;
#endif
    xf::security::aesEnc<_keyWidth> cipher;
    cipher.updateKey(key_r);
    // register IV
    ap_uint<128> IV = IV_strm.read();
#ifndef __SYNTHESIS__
    std::cout << "initialization_vector = " << std::hex << IV << std::endl << std::endl;
#endif

    // intermediate registers to perform the decryption chain
    ap_uint<128> ciphertext_r = 0;
    ap_uint<128> plaintext_r = 0;
    ap_uint<128> feedback_r = 0;
    ap_uint<128> input_block = 0;
    ap_uint<128> output_block = 0;

    // set the initialization for ture
    bool initialization = true;

    bool e = ciphertext_e.read();

decryption_ofb_loop:
    while (!e) {
#pragma HLS PIPELINE
        // read a block of ciphertext, 128 bits
        ciphertext_r = ciphertext.read();
#ifndef __SYNTHESIS__
        std::cout << "ciphertext    = " << std::hex << ciphertext_r << std::endl;
#endif

        // calculate input_block
        if (initialization) { // first iteration, input_block is IV
            input_block = IV;
            initialization = false;
        } else { // after first iteration, input_block is output_block of last iteration
            input_block = feedback_r;
        }
#ifndef __SYNTHESIS__
        std::cout << "input_block  = " << std::hex << input_block << std::endl;
#endif

        // CIPH_k
        cipher.process(input_block, key_r, output_block);
// xf::security::internal::aesEncrypt<_keyWidth>(input_block, key_r, output_block);
#ifndef __SYNTHESIS__
        std::cout << "output_block = " << std::hex << output_block << std::endl;
#endif

        // feedback for the next iteration and get the plaintext for current interation
        feedback_r = output_block;
        plaintext_r = ciphertext_r ^ output_block;
#ifndef __SYNTHESIS__
        std::cout << "plaintext   = " << std::hex << plaintext_r << std::endl;
        std::cout << "feedback     = " << std::hex << feedback_r << std::endl;
#endif

        // write out plaintext
        plaintext.write(plaintext_r);
        plaintext_e.write(0);

        e = ciphertext_e.read();
    }

    plaintext_e.write(1);

} // end aesOfbDecrypt

} // namespace details

/**
 *
 * @brief desOfbEncrypt is OFB encryption mode with DES single block cipher.
 *
 * The algorithm reference is : "Recommendation for Block Cipher Modes of Operation - Methods and Techniques"
 * The implementation is modified for better performance.
 *
 * @param plaintextStrm Input block stream text to be encrypted, each block is 64 bits.
 * @param endPlaintextStrm End flag of block stream plaintext, 1 bit.
 * @param cipherkeyStrm Input cipher key used in encryption, 64 bits for each key.
 * @param IVStrm Initialization vector for the fisrt iteration of DES encrypition, 64 bits.
 * @param ciphertextStrm Output encrypted block stream text, each block is 64 bits.
 * @param endCiphertextStrm End flag of block stream ciphertext, 1 bit.
 *
 */

static void desOfbEncrypt(
    // stream in
    hls::stream<ap_uint<64> >& plaintextStrm,
    hls::stream<bool>& endPlaintextStrm,
    // input cipherkey and initialization vector
    hls::stream<ap_uint<64> >& cipherkeyStrm,
    hls::stream<ap_uint<64> >& IVStrm,
    // stream out
    hls::stream<ap_uint<64> >& ciphertextStrm,
    hls::stream<bool>& endCiphertextStrm) {
    // register cipherkey
    ap_uint<64> key_r = cipherkeyStrm.read();
#ifndef __SYNTHESIS__
    std::cout << std::endl << "cipherkey = " << std::hex << key_r << std::endl;
#endif

    // register IV
    ap_uint<64> IV = IVStrm.read();
#ifndef __SYNTHESIS__
    std::cout << "initialization_vector = " << std::hex << IV << std::endl << std::endl;
#endif

    // intermediate registers to perform the encryption chain
    ap_uint<64> plaintext_r = 0;
    ap_uint<64> feedback_r = 0;
    ap_uint<64> input_block = 0;
    ap_uint<64> output_block = 0;
    ap_uint<64> ciphertext_r = 0;

    // set the initialization for ture
    bool initialization = true;

    bool e = endPlaintextStrm.read();

encryption_ofb_loop:
    while (!e) {
#pragma HLS PIPELINE
        // read a block of plaintext, 64 bits
        plaintext_r = plaintextStrm.read();
#ifndef __SYNTHESIS__
        std::cout << "plaintext    = " << std::hex << plaintext_r << std::endl;
#endif

        // calculate input_block
        if (initialization) { // first iteration, input_block is IV
            input_block = IV;
            initialization = false;
        } else { // after first iteration, input_blcok is output_block of last iteration
            input_block = feedback_r;
        }
#ifndef __SYNTHESIS__
        std::cout << "input_block  = " << std::hex << input_block << std::endl;
#endif

        // CIPH_k
        xf::security::desEncrypt(input_block, key_r, output_block);
#ifndef __SYNTHESIS__
        std::cout << "output_block = " << std::hex << output_block << std::endl;
#endif

        // feedback for the next iteration and get the ciphertext for current interation
        ciphertext_r = plaintext_r ^ output_block;
        feedback_r = output_block;
#ifndef __SYNTHESIS__
        std::cout << "feedback     = " << std::hex << feedback_r << std::endl;
        std::cout << "ciphertext   = " << std::hex << ciphertext_r << std::endl;
#endif

        // write out ciphertext
        ciphertextStrm.write(ciphertext_r);
        endCiphertextStrm.write(0);

        e = endPlaintextStrm.read();
    }

    endCiphertextStrm.write(1);

} // end desOfbEncrypt

/**
 *
 * @brief desOfbDecrypt is OFB decryption mode with DES single block cipher.
 *
 * The algorithm reference is : "Recommendation for Block Cipher Modes of Operation - Methods and Techniques"
 * The implementation is modified for better performance.
 *
 * @param ciphertextStrm Input block stream to be decrypted, each block is 64 bits.
 * @param endCiphertextStrm End flag of block stream ciphertext, 1 bit.
 * @param cipherkeyStrm Input cipher key used in decryption, 64 bits for each key.
 * @param IVStrm Initialization vector for the fisrt iteration of DES decrypition, 64 bits.
 * @param plaintextStrm Output decrypted block stream text, each block is 64 bits.
 * @param endPlaintextStrm End flag of block stream plaintext, 1 bit.
 *
 */

static void desOfbDecrypt(
    // stream in
    hls::stream<ap_uint<64> >& ciphertextStrm,
    hls::stream<bool>& endCiphertextStrm,
    // input cipherkey & initialization vector
    hls::stream<ap_uint<64> >& cipherkeyStrm,
    hls::stream<ap_uint<64> >& IVStrm,
    // stream out
    hls::stream<ap_uint<64> >& plaintextStrm,
    hls::stream<bool>& endPlaintextStrm) {
    // register cipherkey
    ap_uint<64> key_r = cipherkeyStrm.read();
#ifndef __SYNTHESIS__
    std::cout << std::endl << "cipherkey = " << std::hex << key_r << std::endl;
#endif

    // register IV
    ap_uint<64> IV = IVStrm.read();
#ifndef __SYNTHESIS__
    std::cout << "initialization_vector = " << std::hex << IV << std::endl << std::endl;
#endif

    // intermediate registers to perform the decryption chain
    ap_uint<64> ciphertext_r = 0;
    ap_uint<64> plaintext_r = 0;
    ap_uint<64> feedback_r = 0;
    ap_uint<64> input_block = 0;
    ap_uint<64> output_block = 0;

    // set the initialization for ture
    bool initialization = true;

    bool e = endCiphertextStrm.read();

decryption_ofb_loop:
    while (!e) {
#pragma HLS PIPELINE
        // read a block of ciphertext, 64 bits
        ciphertext_r = ciphertextStrm.read();
#ifndef __SYNTHESIS__
        std::cout << "ciphertext    = " << std::hex << ciphertext_r << std::endl;
#endif

        // calculate input_block
        if (initialization) { // first iteration, input_block is IV
            input_block = IV;
            initialization = false;
        } else { // after first iteration, input_block is output_block of last iteration
            input_block = feedback_r;
        }
#ifndef __SYNTHESIS__
        std::cout << "input_block  = " << std::hex << input_block << std::endl;
#endif

        // CIPH_k
        xf::security::desEncrypt(input_block, key_r, output_block);
#ifndef __SYNTHESIS__
        std::cout << "output_block = " << std::hex << output_block << std::endl;
#endif

        // feedback for the next iteration and get the plaintext for current interation
        feedback_r = output_block;
        plaintext_r = ciphertext_r ^ output_block;
#ifndef __SYNTHESIS__
        std::cout << "plaintext   = " << std::hex << plaintext_r << std::endl;
        std::cout << "feedback     = " << std::hex << feedback_r << std::endl;
#endif

        // write out plaintext
        plaintextStrm.write(plaintext_r);
        endPlaintextStrm.write(0);

        e = endCiphertextStrm.read();
    }

    endPlaintextStrm.write(1);

} // end desOfbDecrypt

/**
 *
 * @brief aes128OfbEncrypt is OFB encryption mode with AES-128 single block cipher.
 *
 * The algorithm reference is : "Recommendation for Block Cipher Modes of Operation - Methods and Techniques"
 * The implementation is modified for better performance.
 *
 * @param plaintextStrm Input block stream text to be encrypted, each block is 128 bits.
 * @param endPlaintextStrm End flag of block stream plaintext, 1 bit.
 * @param cipherkeyStrm Input cipher key used in encryption, 128 bits.
 * @param IVStrm Initialization vector for the fisrt iteration of AES encrypition, 128 bits.
 * @param ciphertextStrm Output encrypted block stream text, each block is 128 bits.
 * @param endCiphertextStrm End flag of block stream ciphertext, 1 bit.
 *
 */

static void aes128OfbEncrypt(
    // stream in
    hls::stream<ap_uint<128> >& plaintextStrm,
    hls::stream<bool>& endPlaintextStrm,
    // input cipherkey and initialization vector
    hls::stream<ap_uint<128> >& cipherkeyStrm,
    hls::stream<ap_uint<128> >& IVStrm,
    // stream out
    hls::stream<ap_uint<128> >& ciphertextStrm,
    hls::stream<bool>& endCiphertextStrm) {
    details::aesOfbEncrypt<128>(plaintextStrm, endPlaintextStrm, cipherkeyStrm, IVStrm, ciphertextStrm,
                                endCiphertextStrm);

} // end aes128OfbEncrypt

/**
 *
 * @brief aes128OfbDecrypt is OFB decryption mode with AES-128 single block cipher.
 *
 * The algorithm reference is : "Recommendation for Block Cipher Modes of Operation - Methods and Techniques"
 * The implementation is modified for better performance.
 *
 * @param ciphertextStrm Input block stream to be decrypted, each block is 128 bits.
 * @param endCiphertextStrm End flag of block stream ciphertext, 1 bit.
 * @param cipherkeyStrm Input cipher key used in decryption, 128 bits.
 * @param IVStrm Initialization vector for the fisrt iteration of AES decrypition, 128 bits.
 * @param plaintextStrm Output decrypted block stream text, each block is 128 bits.
 * @param endPlaintextStrm End flag of block stream plaintext, 1 bit.
 *
 */

static void aes128OfbDecrypt(
    // stream in
    hls::stream<ap_uint<128> >& ciphertextStrm,
    hls::stream<bool>& endCiphertextStrm,
    // input cipherkey & initialization vector
    hls::stream<ap_uint<128> >& cipherkeyStrm,
    hls::stream<ap_uint<128> >& IVStrm,
    // stream out
    hls::stream<ap_uint<128> >& plaintextStrm,
    hls::stream<bool>& endPlaintextStrm) {
    details::aesOfbDecrypt<128>(ciphertextStrm, endCiphertextStrm, cipherkeyStrm, IVStrm, plaintextStrm,
                                endPlaintextStrm);

} // end aes128OfbDecrypt

/**
 *
 * @brief aes192OfbEncrypt is OFB encryption mode with AES-192 single block cipher.
 *
 * The algorithm reference is : "Recommendation for Block Cipher Modes of Operation - Methods and Techniques"
 * The implementation is modified for better performance.
 *
 * @param plaintextStrm Input block stream text to be encrypted, each block is 128 bits.
 * @param endPlaintextStrm End flag of block stream plaintext, 1 bit.
 * @param cipherkeyStrm Input cipher key used in encryption, 192 bits.
 * @param IVStrm Initialization vector for the fisrt iteration of AES encrypition, 128 bits.
 * @param ciphertextStrm Output encrypted block stream text, each block is 128 bits.
 * @param endCiphertextStrm End flag of block stream ciphertext, 1 bit.
 *
 */

static void aes192OfbEncrypt(
    // stream in
    hls::stream<ap_uint<128> >& plaintextStrm,
    hls::stream<bool>& endPlaintextStrm,
    // input cipherkey and initialization vector
    hls::stream<ap_uint<192> >& cipherkeyStrm,
    hls::stream<ap_uint<128> >& IVStrm,
    // stream out
    hls::stream<ap_uint<128> >& ciphertextStrm,
    hls::stream<bool>& endCiphertextStrm) {
    details::aesOfbEncrypt<192>(plaintextStrm, endPlaintextStrm, cipherkeyStrm, IVStrm, ciphertextStrm,
                                endCiphertextStrm);

} // end aes192OfbEncrypt

/**
 *
 * @brief aes192OfbDecrypt is OFB decryption mode with AES-192 single block cipher.
 *
 * The algorithm reference is : "Recommendation for Block Cipher Modes of Operation - Methods and Techniques"
 * The implementation is modified for better performance.
 *
 * @param ciphertextStrm Input block stream to be decrypted, each block is 128 bits.
 * @param endCiphertextStrm End flag of block stream ciphertext, 1 bit.
 * @param cipherkeyStrm Input cipher key used in decryption, 192 bits.
 * @param IVStrm Initialization vector for the fisrt iteration of AES decrypition, 128 bits.
 * @param plaintextStrm Output decrypted block stream text, each block is 128 bits.
 * @param endPlaintextStrm End flag of block stream plaintext, 1 bit.
 *
 */

static void aes192OfbDecrypt(
    // stream in
    hls::stream<ap_uint<128> >& ciphertextStrm,
    hls::stream<bool>& endCiphertextStrm,
    // input cipherkey & initialization vector
    hls::stream<ap_uint<192> >& cipherkeyStrm,
    hls::stream<ap_uint<128> >& IVStrm,
    // stream out
    hls::stream<ap_uint<128> >& plaintextStrm,
    hls::stream<bool>& endPlaintextStrm) {
    details::aesOfbDecrypt<192>(ciphertextStrm, endCiphertextStrm, cipherkeyStrm, IVStrm, plaintextStrm,
                                endPlaintextStrm);

} // end aes192OfbDecrypt

/**
 *
 * @brief aes256OfbEncrypt is OFB encryption mode with AES-256 single block cipher.
 *
 * The algorithm reference is : "Recommendation for Block Cipher Modes of Operation - Methods and Techniques"
 * The implementation is modified for better performance.
 *
 * @param plaintextStrm Input block stream text to be encrypted, each block is 128 bits.
 * @param endPlaintextStrm End flag of block stream plaintext, 1 bit.
 * @param cipherkeyStrm Input cipher key used in encryption, 256 bits.
 * @param IVStrm Initialization vector for the fisrt iteration of AES encrypition, 128 bits.
 * @param ciphertextStrm Output encrypted block stream text, each block is 128 bits.
 * @param endCiphertextStrm End flag of block stream ciphertext, 1 bit.
 *
 */

static void aes256OfbEncrypt(
    // stream in
    hls::stream<ap_uint<128> >& plaintextStrm,
    hls::stream<bool>& endPlaintextStrm,
    // input cipherkey and initialization vector
    hls::stream<ap_uint<256> >& cipherkeyStrm,
    hls::stream<ap_uint<128> >& IVStrm,
    // stream out
    hls::stream<ap_uint<128> >& ciphertextStrm,
    hls::stream<bool>& endCiphertextStrm) {
    details::aesOfbEncrypt<256>(plaintextStrm, endPlaintextStrm, cipherkeyStrm, IVStrm, ciphertextStrm,
                                endCiphertextStrm);

} // end aes256OfbEncrypt

/**
 *
 * @brief aes256OfbDecrypt is OFB decryption mode with AES-256 single block cipher.
 *
 * The algorithm reference is : "Recommendation for Block Cipher Modes of Operation - Methods and Techniques"
 * The implementation is modified for better performance.
 *
 * @param ciphertextStrm Input block stream to be decrypted, each block is 128 bits.
 * @param endCiphertextStrm End flag of block stream ciphertext, 1 bit.
 * @param cipherkeyStrm Input cipher key used in decryption, 256 bits.
 * @param IVStrm Initialization vector for the fisrt iteration of AES decrypition, 128 bits.
 * @param plaintextStrm Output decrypted block stream text, each block is 128 bits.
 * @param endPlaintextStrm End flag of block stream plaintext, 1 bit.
 *
 */

static void aes256OfbDecrypt(
    // stream in
    hls::stream<ap_uint<128> >& ciphertextStrm,
    hls::stream<bool>& endCiphertextStrm,
    // input cipherkey & initialization vector
    hls::stream<ap_uint<256> >& cipherkeyStrm,
    hls::stream<ap_uint<128> >& IVStrm,
    // stream out
    hls::stream<ap_uint<128> >& plaintextStrm,
    hls::stream<bool>& endPlaintextStrm) {
    details::aesOfbDecrypt<256>(ciphertextStrm, endCiphertextStrm, cipherkeyStrm, IVStrm, plaintextStrm,
                                endPlaintextStrm);

} // end aes256OfbDecrypt

} // namespace security
} // namespace xf
#endif
