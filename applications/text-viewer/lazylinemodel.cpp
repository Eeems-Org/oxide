#include "lazylinemodel.h"
#include <QDebug>
#include <QTextStream>

LazyLineModel::LazyLineModel(QObject* parent)
  : QAbstractListModel(parent) {}

LazyLineModel::~LazyLineModel() {
  if (m_file.isOpen()) {
    m_file.close();
  }
}

int
LazyLineModel::rowCount(const QModelIndex&) const {
  QMutexLocker locker(&m_mutex);
  return m_totalLines;
}

QVariant
LazyLineModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || role != LineRole) {
    return QVariant();
  }

  QMutexLocker locker(&m_mutex);
  int row = index.row();
  if (row < 0 || row >= m_totalLines) {
    return QVariant();
  }

  if (row < m_lineCache.size() && !m_lineCache[row].isNull()) {
    return m_lineCache[row];
  }

  m_file.seek(m_lineOffsets[row]);
  QTextStream stream(&m_file);
  stream.setEncoding(QStringConverter::Utf8);
  const_cast<QVector<QString>&>(m_lineCache)[row] = stream.readLine();
  return m_lineCache[row];
}

QHash<int, QByteArray>
LazyLineModel::roleNames() const {
  return {
    {LineRole, "line"}
  };
}

int
LazyLineModel::lineCount() const {
  QMutexLocker locker(&m_mutex);
  return m_totalLines;
}

QString
LazyLineModel::filePath() const {
  return m_filePath;
}

void
LazyLineModel::setFilePath(const QString& path) {
  if (m_filePath == path) {
    return;
  }

  if (m_file.isOpen()) {
    m_file.close();
  }
  m_filePath = path;
  m_lineOffsets.clear();
  m_lineCache.clear();
  m_totalLines = 0;

  m_file.setFileName(path);
  if (m_file.open(QIODevice::ReadOnly)) {
    m_fileSize = m_file.size();
    loadFileIndex();
  }

  emit filePathChanged();
  emit lineCountChanged();
}

void
LazyLineModel::loadFileIndex() {
  m_lineOffsets.clear();
  m_lineOffsets.append(0);

  QByteArray chunk;
  qint64 pos = 0;
  while (pos < m_fileSize) {
    chunk = m_file.read(qMin<qint64>(8192, m_fileSize - pos));
    for (char c : chunk) {
      if (c == '\n') {
        m_lineOffsets.append(pos + 1);
      }
      pos++;
    }
  }
  m_totalLines = m_lineOffsets.size();
  m_lineCache.resize(m_totalLines);
}

void
LazyLineModel::loadLines(int start, int end) const {
  if (!m_file.isOpen() || start >= end)
    return;

  if (m_lineCache.size() < m_totalLines) {
    const_cast<QVector<QString>&>(m_lineCache).resize(m_totalLines);
  }

  QTextStream stream(&m_file);
  stream.setEncoding(QStringConverter::Utf8);

  for (int i = start; i < end; ++i) {
    if (!m_lineCache[i].isNull()) {
      continue;
    }
    m_file.seek(m_lineOffsets[i]);
    const_cast<QVector<QString>&>(m_lineCache)[i] = stream.readLine();
  }

  for (int i = 0; i < m_totalLines; ++i) {
    if (i < start || i >= end) {
      const_cast<QVector<QString>&>(m_lineCache)[i].clear();
    }
  }
}

void
LazyLineModel::ensureLoaded(int start, int end) {
  QMutexLocker locker(&m_mutex);
  start = qMax(0, start);
  end = qMin(m_totalLines, end);
  loadLines(start, end);
}
