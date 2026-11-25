/*!
 * \addtogroup IPC
 * \brief The IPC module
 * @{
 * \file
 */
#pragma once

#include "liboxide_global.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QList>
#include <QLocalServer>
#include <QObject>
#include <QString>
#include <QStringList>

#define ipc Oxide::IPC::instance()

namespace Oxide {

  /*!
   * \class IPC
   * \brief IPC helper that provides a simple way to ensure only one instance of
   * an application runs and to send data (file lists, raw messages, or file
   * descriptors) to that primary instance.
   *
   * Example:
   * \code
   * int main(int argc, char* argv[]) {
   *     QGuiApplication app(argc, argv);
   *     app.setApplicationName("codes.eeems.myapp");
   *     if (!ipc->start()) {
   *         QStringList files = {"file1.txt", "file2.txt"};
   *         ipc->send(files);
   *         return EXIT_SUCCESS;
   *     }
   *     QObject::connect(&ipc, &Oxide::IPC::files, [](const QStringList& files)
   * {
   *         qDebug() << "Files" << files;
   *     });
   *     return app.exec();
   * }
   * \endcode
   */
  class LIBOXIDE_EXPORT IPC : public QObject {
    Q_OBJECT

  public:
    /*!
     * \brief Construct an IPC object using the QCoreApplication's
     * applicationName
     * \param app QCoreApplication instance (used for applicationName)
     * \param parent QObject parent
     */
    IPC(QCoreApplication* app, QObject* parent = nullptr);

    /*!
     * \brief Construct an IPC object with a custom application name
     * \param name Unique application identifier (used for lock file and
     * socket)
     * \param parent QObject parent
     */
    explicit IPC(const QString& name, QObject* parent = nullptr);

    /*!
     * \brief Get the global IPC singleton instance
     * \return IPC instance or nullptr if no QCoreApplication exists.
     */
    static IPC* instance();

    ~IPC();

    /*!
     * \brief Start the primary instance IPC server
     * \return true if this instance is the primary, false if another instance
     * holds the lock
     */
    bool start();

    /*!
     * \brief Send a file list to the primary instance
     * \param files List of file paths to send
     * \param timeoutMs Timeout in milliseconds
     * \return true if the primary instance acknowledged receipt
     */
    bool send(const QStringList& files, int timeoutMs = 2000);

    /*!
     * \brief Send a message to the primary instance
     * \param data Raw message data
     * \param timeoutMs Timeout in milliseconds
     * \return true if the primary instance acknowledged receipt
     */
    bool send(const QByteArray& data, int timeoutMs = 2000);

  signals:
    /*!
     * \brief Emitted when a file list is received from a secondary instance
     * \param files List of file paths sent by the secondary instance
     */
    void files(const QStringList& files);

    /*!
     * \brief Emitted when a message is received from a secondary instance
     * \param data message data
     */
    void message(const QByteArray& data);

  private:
    QString m_name;
    QString m_lockPath;
    QString m_socketPath;
    int m_lockFd = -1;
    QLocalServer* m_server = nullptr;
  };

} // namespace Oxide
/*! @} */
