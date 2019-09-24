#include <QDebug>
#include "appitem.h"
#include <iostream>
#include <fstream>
#include <stdlib.h>

bool AppItem::ok(){
    return !_name.isEmpty() && !_call.isEmpty();
}

void AppItem::execute(){
    qDebug() << "Setting termfile /opt/etc/draft/.terminate";
    std::ofstream termfile;
    termfile.open("/opt/etc/draft/.terminate");
    termfile << _term.toStdString() << std::endl;
    system(_call.toUtf8());
}
