/*!
 * \addtogroup Oxide
 * @{
 * \file
 */
#pragma once
#include "liboxide_global.h"
#include <QLocalSocket>

namespace Oxide{
    /*!
     * \brief A socket pair used for two way communication
     */
    class LIBOXIDE_EXPORT SocketPair : public QObject{
        Q_OBJECT

    public:
        /*!
         * \brief Create a new socket pair
         * \param allowWriteSocketRead Allow reading from the write socket, this allows messages to be sent by the other end of the socket
         */
        SocketPair(bool allowWriteSocketRead = false);
        ~SocketPair();
        /*!
         * \brief Is this socket pair valid
         * \return If this socket pair valid
         */
        bool isValid();
        /*!
         * \brief Is this socket pair readable
         * \return If this socket pair readable
         */
        bool isReadable();
        /*!
         * \brief Is this socket pair open
         * \return If this socket pair open
         */
        bool isOpen();
        /*!
         * \brief The QLocalSocket instance of the read socket
         * \return The QLocalSocket instance of the read socket
         */
        QLocalSocket* readSocket();
        /*!
         * \brief The QLocalSocket instance of the write socket
         * \return The QLocalSocket instance of the write socket
         */
        QLocalSocket* writeSocket();
        /*!
         * \brief Enable or disable reading and writing to this socket pair
         * \param enabled If reading and writing to this socket pair is enabled
         */
        void setEnabled(bool enabled);
        /*!
         * \brief Is reading and writing to this socket pair is enabled
         * \return If reading and writing to this socket pair is enabled
         */
        bool enabled();
        /*!
         * \brief Is there more data available to read from the read socket
         * \return If there more data available to read from the read socket
         */
        bool atEnd();
        /*!
         * \brief Read a line from the read socket
         * \param maxlen Maxiumum amount of data to read
         * \return The line of data
         */
        QByteArray readLine(qint64 maxlen = 0);
        /*!
         * \brief Read all available data on the read socket
         * \return The data
         */
        QByteArray readAll();
        /*!
         * \brief Read data from the read socket
         * \param maxlen maximum amount of data to read
         * \return The data
         */
        QByteArray read(qint64 maxlen = 0);
        /*!
         * \brief How many bytes are available to read from the read socket
         * \return How many bytes that are available to read from the read socket
         */
        qint64 bytesAvailable();
        qint64 _write(const char* data, qint64 size);
        /*!
         * \brief Write data to the write socket
         * \param data Data to write
         * \param size Size of data to write
         * \return Amount of data written
         */
        qint64 write(const char* data, qint64 size);
        qint64 _write(QByteArray data);
        /*!
         * \brief Write data to the write socket
         * \param data Data to write
         * \return Amount of data written
         */
        qint64 write(QByteArray data);
        /*!
         * \brief Error string if the last operation on the write socket errored
         * \return Error string if the last operation on the write socket errored
         */
        QString errorString();

    signals:
        /*!
         * \brief The read socket has data available to read
         */
        void readyRead();
        /*!
         * \brief Data was written to the write socket
         * \param bytes Amount of data written
         */
        void bytesWritten(qint64 bytes);
        /*!
         * \brief The write socket has disconnected
         */
        void disconnected();

    public slots:
        /*!
         * \brief Close both the read and write sockets
         */
        void close();

    private slots:
        void _readyRead();

    private:
        QLocalSocket m_readSocket;
        QLocalSocket m_writeSocket;
        bool m_enabled;
        bool m_allowWriteSocketRead;
    };
}
/*! @} */
