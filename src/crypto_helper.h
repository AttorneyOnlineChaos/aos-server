#pragma once

#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QString>

namespace kenji
{
/**
 * @brief Simple header library for basic cryptographic functionality
 */
class CryptoHelper
{
private:
  /**
   * @brief Length of the output of PBKDF2
   */
  static constexpr qint32 pbkdf2_output_len = 32; // 32 bytes (SHA-256)
  /**
   * @brief Configurable cost parameter of PBKDF2
   */
  static constexpr quint32 pbkdf2_cost = 100000;

  /**
   * @brief Compute the SHA-256 HMAC of given data
   *
   * @param salt HMAC key
   * @param password HMAC data
   * @return QByteArray HMAC result
   */
  static QByteArray hmac(const QByteArray &salt, const QByteArray &password)
  {
    QMessageAuthenticationCode hmac(QCryptographicHash::Sha256);
    hmac.setKey(salt);
    hmac.addData(password);
    return hmac.result();
  }

  /**
   * @brief Compute the SHA-256 HMAC of strings
   *
   * @param salt Salt value
   * @param password Password value
   * @return QString HMAC result, hex encoded
   */
  static QString password_hmac(const QString &salt, const QString &password) { return hmac(salt.toUtf8(), password.toUtf8()).toHex(); }

  /**
   * @brief Perform the PBKDF2 key-derivation function
   *
   * @details PBKDF2 is a very configurable algorithm, but most of its functionality
   * does not apply here. Instead, we fix the output to the size of the underlying
   * hash function, which greatly simplifies the algorithm.
   *
   * @param salt Salt value
   * @param password Password value
   * @return QString PBKDF2 result, hex encoded
   */
  static QString pbkdf2(const QByteArray &salt, const QString &password)
  {
    QByteArray bigendian_one("\x00\x00\x00\x01", 4);
    QByteArray last_block = salt;
    last_block.append(bigendian_one);

    QByteArray result(pbkdf2_output_len, '\0');

    for (unsigned int i = 0; i < pbkdf2_cost; i++)
    {
      last_block = hmac(password.toUtf8(), last_block);
      for (unsigned int n = 0; n < pbkdf2_output_len; n++)
      {
        result[n] = result[n] ^ last_block[n];
      }
    }

    return result.toHex();
  }

public:
  /**
   * @brief The length of the salt value used for PBKDF2, in bytes
   */
  static constexpr qint32 pbkdf2_salt_len = 16;

  /**
   * @brief Compute the hash of a password to store, given a salt
   *
   * @details This function selects one of two hashing algorithm backends to use,
   * dependent on the length of the salt. Older versions of this program used an
   * 8-byte salt, but newer versions use a 16-byte salt. This allows us to version
   * the algorithm being used without the need to change the schema of the database.
   *
   * @param salt Salt value
   * @param password Password value
   * @return QString Hashed password, hex encoded
   */
  static QString hash_password(const QByteArray &salt, const QString &password)
  {
    // Select the correct hash backend based on the salt length
    if (salt.length() < pbkdf2_salt_len)
    {
      // Due to an implementation oversight, the old HMAC algorithm
      // does not correctly handle salts. Instead of treating the hex string
      // as binary data, it is used directly. We have to handle this case.
      return password_hmac(salt.toHex(), password);
    }

    return pbkdf2(salt, password);
  }

  /**
   * @brief Generate a random octet
   *
   * @return quint8 A random 8-bit value
   */
  static quint8 rand8()
  {
    quint32 l_rand = QRandomGenerator::system()->generate();
    return (quint8)(l_rand & 0xFF);
  }

  /**
   * @brief Generate an arbitrary amount of random data
   *
   * @param n Number of bytes to generate
   * @return QByteArray Random bytes
   */
  static QByteArray randbytes(int n)
  {
    QByteArray output;
    for (int i = 0; i < n; i++)
    {
      output.append(CryptoHelper::rand8());
    }
    return output;
  }
};
} // namespace kenji
