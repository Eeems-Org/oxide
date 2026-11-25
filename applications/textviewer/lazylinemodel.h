#pragma once

#include <QAbstractListModel>
#include <QFile>
#include <QMutex>
#include <QString>
#include <QVector>

class LazyLineModel : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int lineCount READ lineCount NOTIFY lineCountChanged)
  Q_PROPERTY(
    QString filePath READ filePath WRITE setFilePath NOTIFY filePathChanged
  )

public:
  enum Roles { LineRole = Qt::UserRole + 1 };

  explicit LazyLineModel(QObject* parent = nullptr);
  ~LazyLineModel();

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant
  data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  int lineCount() const;
  QString filePath() const;
  void setFilePath(const QString& path);

  Q_INVOKABLE void ensureLoaded(int start, int end);

signals:
  void lineCountChanged();
  void filePathChanged();

private:
  void loadFileIndex();
  void loadLines(int start, int end) const;
  mutable QMutex m_mutex;
  QString m_filePath;
  mutable QFile m_file;
  qint64 m_fileSize = 0;
  QVector<qint64> m_lineOffsets;
  mutable QVector<QString> m_lineCache;
  int m_totalLines = 0;
};