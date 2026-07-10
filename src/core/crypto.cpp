//
// Created by xint2 on 09/07/2026.
//
#include "../../include/core/crypto.h"
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <stdexcept>
#include <string>
#include <cstring>
#include <random>

namespace sxaint::core {
    std::vector<std::byte> Crypto::derive_key(uint32_t pin) {
        std::vector<std::byte> key(32);// 256 bits for AES-256
        std::string pin_str = std::to_string(pin) + "sxaint_taintx_xsaint";
        SHA256(reinterpret_cast<const unsigned char *>(pin_str.c_str()), pin_str.length(),reinterpret_cast<unsigned char*>(key.data()));
        return key;
    }
    std::vector<std::byte> Crypto::encrypt(std::span<const std::byte> plaintext, const std::vector<std::byte> &key, uint32_t chunk_id) {
        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        if (!ctx) throw std::runtime_error("failed to create EVP context");
        std::vector<std::byte> ciphertext(plaintext.size() + 16);
        int len = 0;
        int ciphertext_len = 0;
        unsigned char iv[12] = {0};
        std::memcpy(iv, &chunk_id, sizeof(chunk_id));
        
        if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, reinterpret_cast<const unsigned char *>(key.data()), iv)) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("EVP_EncryptInit_ex failed");
        }
        
        if (1 != EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char *>(ciphertext.data()), &len, reinterpret_cast<const unsigned char*>(plaintext.data()), plaintext.size())) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("EVP_EncryptUpdate failed");
        }
        ciphertext_len = len;
        
        if (1 != EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char *>(ciphertext.data()) + len, &len)) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("EVP_EncryptFinal_ex failed");
        }
        ciphertext_len += len;

        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, reinterpret_cast<unsigned char*>(ciphertext.data()) + ciphertext_len);
        EVP_CIPHER_CTX_free(ctx);
        ciphertext.resize(ciphertext_len + 16);
        return ciphertext;
    }
    std::vector<std::byte> Crypto::decrypt(std::span<const std::byte> ciphertext, const std::vector<std::byte> &key, uint32_t chunk_id) {
        if (ciphertext.size()< 16) throw std::runtime_error("Ciphertext too small to contain MAC");
        EVP_CIPHER_CTX *ctx =EVP_CIPHER_CTX_new();
        if (!ctx) throw std::runtime_error("Failed to create EVP context");
        size_t actual_cipher_len = ciphertext.size() - 16;
        std::vector<std::byte> plaintext(actual_cipher_len);
        int len = 0;
        int plaintext_len = 0;

        unsigned char iv[12] = {0};
        std::memcpy(iv, &chunk_id, sizeof(chunk_id));

        if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, reinterpret_cast<const unsigned char *>(key.data()), iv)) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("EVP_DecryptInit_ex failed");
        }
        
        if (1 != EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(plaintext.data()), &len, reinterpret_cast<const unsigned char*>(ciphertext.data()), actual_cipher_len)) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("EVP_DecryptUpdate failed");
        }
        plaintext_len = len;
        
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(ciphertext.data() + actual_cipher_len)));

        int ret = EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char *>(plaintext.data()) + len, &len);
        EVP_CIPHER_CTX_free(ctx);
        if (ret > 0) {
            plaintext_len += len;
            plaintext.resize(plaintext_len); // Ensure exact size
            return plaintext;
        } else {
            throw std::runtime_error("MAC verification failed! Data was tampered with or PIN is wrong.");
        }

    }




}