// Copyright (c) 2023 Manuel Schneider

#pragma once
#include "albert/extension/pluginprovider/plugininstance.h"
#include "albert/extension/pluginprovider/pluginloader.h"
#include "albert/extension/pluginprovider/pluginmetadata.h"
#include "albert/extensionregistry.h"
#include "albert/util/timeprinter.h"
#include "plugininstanceprivate.h"
#include <QCoreApplication>
using namespace albert;
using namespace std;

class PluginLoader::PluginLoaderPrivate
{
public:

    PluginLoader *loader;
    QString state_info{};
    PluginState state{PluginState::Unloaded};

    PluginLoaderPrivate(PluginLoader *l) : loader(l)
    {
    }

    void setState(PluginState s, QString info = {})
    {
        state = s;
        state_info = info;
        emit loader->stateChanged();
    }

    QString load(ExtensionRegistry *registry)
    {
        switch (state) {
            case PluginState::Loaded:
                return QStringLiteral("Plugin is already loaded.");
            case PluginState::Busy:
                return QStringLiteral("Plugin is currently busy.");
            case PluginState::Unloaded:
            {
                setState(PluginState::Busy, QStringLiteral("Loading…"));
                TimePrinter tp(QString("[%1 ms] spent loading plugin '%2'").arg("%1", loader->metaData().id));

                QStringList errors;

                try {
                    QCoreApplication::processEvents();
                    PluginInstancePrivate::in_construction = loader;
                    if (auto err = loader->load(); err.isEmpty()){
                        if (auto *p_instance = loader->instance()){

                            QCoreApplication::processEvents();
                            p_instance->initialize(registry);

                            QCoreApplication::processEvents();
                            for (auto *e : p_instance->extensions())
                                registry->add(e);

                            setState(PluginState::Loaded);
                            return {};

                        } else
                            errors << "Plugin laoded successfully but returned nullptr instance.";
                    } else
                        errors << err;

                    if (auto err = loader->unload(); !err.isNull())
                        errors << err;

                } catch (const exception& e) {
                    errors << e.what();
                } catch (...) {
                    errors << "Unknown exception while loading plugin.";
                }

                auto err_str = errors.join("\n");
                setState(PluginState::Unloaded, err_str);
                return err_str;
            }
        }
        return {};
    }

    QString unload(ExtensionRegistry *registry)
    {
        switch (state) {
            case PluginState::Unloaded:
                return QStringLiteral("Plugin is not loaded.");
            case PluginState::Busy:
                return QStringLiteral("Plugin is currently busy.");
            case PluginState::Loaded:
            {
                setState(PluginState::Busy, QStringLiteral("Loading…"));
                TimePrinter tp(QString("[%1 ms] spent unloading plugin '%2'").arg("%1", loader->metaData().id));

                QCoreApplication::processEvents();

                QStringList errors;

                if (auto *p_instance = loader->instance()){

                    for (auto *e : p_instance->extensions())
                        registry->remove(e);

                    QCoreApplication::processEvents();

                    try {
                        p_instance->finalize(registry);

                        QCoreApplication::processEvents();

                        if (auto err = loader->unload(); !err.isNull())
                            errors << err;

                    } catch (const exception& e) {
                        errors << e.what();
                    } catch (...) {
                        errors << "Unknown exception while unloading plugin.";
                    }
                } else
                    errors << "Plugin laoded but returned nullptr instance.";

                auto err_str = errors.join("\n");
                setState(PluginState::Unloaded, err_str);
                return err_str;
            }
        }
        return {};
    }
};