// Copyright (c) 2022 Manuel Schneider

#pragma once
#include "albert.h"
#include "export.h"
#include <QObject>
#include <set>

namespace albert
{
class Extension;
template<class T>
class ALBERT_EXPORT ExtensionWatcher  /// Non-QObject extension registry observer
{
public:
    ExtensionWatcher()
    {
        auto &registry = albert::extensionRegistry();

        conn_r = QObject::connect(&registry, &ExtensionRegistry::added, [this](Extension *e){
            if (T *t = dynamic_cast<T*>(e)){
                extensions_.insert(t);
                onAdd(t);
            }
        });

        conn_u = QObject::connect(&registry, &ExtensionRegistry::removed, [this](Extension *e){
            if (T *t = dynamic_cast<T*>(e)){
                extensions_.erase(t);
                onRem(t);
            }
        });

        for (auto &[id, e] : registry.extensionsOfType<T>())
            extensions_.insert(e);
    }

    ~ExtensionWatcher()
    {
        QObject::disconnect(conn_r);
        QObject::disconnect(conn_u);
    }

protected:

    virtual void onAdd(T *) {}
    virtual void onRem(T *) {}

    const std::set<T*> &extensions() const { return extensions_; }

private:
    std::set<T*> extensions_;
    QMetaObject::Connection conn_r;
    QMetaObject::Connection conn_u;
};
}
