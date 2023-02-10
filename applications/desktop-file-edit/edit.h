#pragma once
#ifndef EDIT_H
#define EDIT_H

#include <QCommandLineParser>
#include <QJsonObject>

void addEditOptions(QCommandLineParser& parser);
bool validateSetKeyValueOptions(QCommandLineParser& parser);
void applyChanges(QCommandLineParser& parser, QJsonObject& reg, const QString& name);

#endif // EDIT_H
