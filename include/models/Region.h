#ifndef REGION_H
#define REGION_H

#include <QString>

class Region {
  public:
    int id;
    QString name;

    Region() : id(0) {}
    Region(int id, QString name) : id(id), name(name) {}
};

#endif
