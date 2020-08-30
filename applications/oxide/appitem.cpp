#include <QDebug>
#include "appitem.h"
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <unistd.h>

bool AppItem::ok(){
    return !_name.isEmpty() && !_call.isEmpty();
}

void AppItem::execute(){
    qDebug() << "Setting termfile /tmp/.terminate";
    std::ofstream termfile;
    termfile.open("/tmp/.terminate");
    termfile << _term.toStdString() << std::endl;
    system(_call.toUtf8());
}
