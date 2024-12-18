#ifndef SECURE_SEND_HPP
#define SECURE_SEND_HPP

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstring>

using boost::asio::ip::tcp;
using json = nlohmann::json;

class SecureSender {
public:
    //generate random AES key
    static std::vector<unsigned char> generate_aes_key() {
        std::vector<unsigned char> key(AES_BLOCK_SIZE);
        if (!RAND_bytes(key.data(), AES_BLOCK_SIZE)) {
            throw std::runtime_error("Failed to generate AES key");
        }
        return key;
    }

    // Encrypts data with AES
    static std::vector<unsigned char> aes_encrypt(const std::string& plaintext, const std::vector<unsigned char>& key) {
        if (key.size() != AES_BLOCK_SIZE) {
            throw std::invalid_argument("AES key must be 128 bits (16 bytes)");
        }

        AES_KEY encryptKey;
        if (AES_set_encrypt_key(key.data(), 128, &encryptKey) < 0) {
            throw std::runtime_error("Failed to set AES encryption key");
        }

        size_t paddedSize = ((plaintext.size() + AES_BLOCK_SIZE - 1) / AES_BLOCK_SIZE) * AES_BLOCK_SIZE;
        std::vector<unsigned char> paddedData(paddedSize, 0);
        std::memcpy(paddedData.data(), plaintext.data(), plaintext.size());

        std::vector<unsigned char> ciphertext(paddedSize);
        for (size_t i = 0; i < paddedSize; i += AES_BLOCK_SIZE) {
            AES_encrypt(paddedData.data() + i, ciphertext.data() + i, &encryptKey);
        }

        return ciphertext;
    }

    static void send_encrypted_key(tcp::socket& socket, int key, const std::vector<unsigned char>& aes_key) {
        try {
            json message = {{"key", key}};
            std::string plaintext = message.dump();

            auto encrypted = aes_encrypt(plaintext, aes_key);

            // Convert encrypted data to a hex string
            std::string encryptedHex;
            for (unsigned char c : encrypted) {
                char buffer[3];
                snprintf(buffer, sizeof(buffer), "%02x", c);
                encryptedHex += buffer;
            }

            encryptedHex += "\n";

            // Send encrypted data
            boost::asio::write(socket, boost::asio::buffer(encryptedHex));
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Failed to send encrypted key: ") + e.what());
        }
    }
};

#endif // SECURE_SEND_HPP
