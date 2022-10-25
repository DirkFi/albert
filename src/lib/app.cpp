//// Copyright (c) 2022 Manuel Schneider

#include "app.h"
#include "logging.h"
#include "pluginprovider.h"
#include "frontend.h"
//#include "settings/settingswindow.h"
#include <QMessageBox>
#include <QSettings>
#include <QDir>
#include "rpcserver.h"
#include <QApplication>
//#ifdef Q_OS_MAC
//#include "macos.h"
//#endif

ALBERT_DEFINE_LOGGING_CATEGORY
using namespace std;
static const char *CFG_LAST_USED_VERSION = "last_used_version";
static const char *CFG_FRONTEND_ID = "frontendId";
static const char *DEF_FRONTEND_ID = "widgetboxmodel";
static albert::App *instance_ = nullptr;

static QStringList defaultPluginDirs()
{
    QStringList pluginDirs;
#if defined __linux__ || defined __FreeBSD__
    QStringList dirs = {
#if defined MULTIARCH_TUPLE
        QFileInfo("/usr/lib/" MULTIARCH_TUPLE).canonicalFilePath(),
#endif
        QFileInfo("/usr/lib/").canonicalFilePath(),
        QFileInfo("/usr/lib64/").canonicalFilePath(),
        QFileInfo("/usr/local/lib/").canonicalFilePath(),
        QFileInfo("/usr/local/lib64/").canonicalFilePath(),
        QDir::home().filePath(".local/lib/"),
        QDir::home().filePath(".local/lib64/")
    };

    dirs.removeDuplicates();

    for ( const QString& dir : dirs ) {
        QFileInfo fileInfo = QFileInfo(QDir(dir).filePath("albert/plugins"));
        if ( fileInfo.isDir() )
            pluginDirs.push_back(fileInfo.canonicalFilePath());
    }
#elif defined __APPLE__
    // TODO deplopyment?
    pluginDirs.push_back(QDir("../lib").canonicalPath());
#elif defined _WIN32
    qFatal("Not implemented");
#endif
    return pluginDirs;
}

struct albert::App::Private
{
    albert::App *q;
    RPCServer rpc_server;
    ::PluginProvider plugin_provider;
    std::map<QString,Extension*> extensions;
    Frontend *frontend;

    void notifyVersionChangeAndFirstRun()
    {
        auto settings = QSettings(qApp->applicationName());
        auto current_version = qApp->applicationVersion();
        auto last_used_version = settings.value(CFG_LAST_USED_VERSION).toString();

        if (last_used_version.isNull()){  // First run
            QMessageBox(
                    QMessageBox::Warning, "First run",
                    "This is the first time you've launched Albert. Albert is plugin based. "
                    "You have to enable extension you want to use. "
                    "Note that you wont be able to open albert without a hotkey or "
                    "tray icon.").exec();
            q->showSettings();
            settings.setValue(CFG_LAST_USED_VERSION, current_version);
        }
        else if (current_version.section('.', 1, 1) != last_used_version.section('.', 1, 1) ){  // FIXME in first major version
            QMessageBox(QMessageBox::Information, "Major version changed",
                        QString("You are now using Albert %1. The major version changed. "
                                "Some parts of the API might have changed. Check the "
                                "<a href=\"https://albertlauncher.github.io/news/\">news</a>.")
                                .arg(current_version)).exec();
        }
    }

    void loadFrontend()
    {
        // Get all specs of type frontend
        std::map<QString, PluginProvider::PluginSpec&> frontends;
        for(auto &[id, spec] : plugin_provider.plugins())
            if(spec.type == PluginProvider::PluginType::Frontend && spec.state == PluginProvider::PluginState::Unloaded)
                frontends.emplace(id, spec);

        // Helper function loading frontend extensions
        auto load_frontend = [&](const QString &id) -> Frontend* {
            if (!plugin_provider.loadPlugin(id))
                return nullptr;  // Loading failed
            try {
                return q->extension<albert::Frontend>(id);
            } catch (const std::out_of_range &e) {
                return nullptr;  // Extension not registered
            }
        };

        // Try loading the configured frontend
        auto cfg_frontend = QSettings(qApp->applicationName()).value(CFG_FRONTEND_ID, DEF_FRONTEND_ID).toString();
        if (frontend = load_frontend(cfg_frontend); frontend)
            return;

        WARN << "Loading configured frontend failed. Try any other.";

        for (auto &[id, spec] : frontends)
            if (frontend = load_frontend(id); frontend) {
                WARN << QString("Using %1 instead.").arg(id);
                return;
            }

        qFatal("Could not load any frontend.");
    }

//    friend class SettingsWindow;
//    QPointer<SettingsWindow> settings_window;
//    QString id() const override { return ""; }
//    QString handleSocketMessage(const QString &message) override;
//    TrayIcon tray_icon;
//    TerminalProvider terminal_provider;
//    QueryEngine query_engine;
};

albert::App::App(const QStringList &additional_plugin_dirs) : d(new Private)
{
    if (instance_ != nullptr)
        qFatal("App created twice");
    instance_ = this;


    d->q = this;
    d->notifyVersionChangeAndFirstRun();
    d->plugin_provider.findPlugins(QStringList(additional_plugin_dirs) << defaultPluginDirs());
    d->loadFrontend();
}

albert::App *albert::App::instance()
{
    return instance_;
}

void albert::App::show(const QString &text)
{
//    if (!text.isNull())
//        native_plugin_provider.frontend().setInput(text);
//    native_plugin_provider.frontend().setVisible(true);
}

void albert::App::showSettings()
{
//    if (!d->settings_window)
//        d->settings_window = new SettingsWindow(*this);
//    d->settings_window->bringToFront();
}

void albert::App::restart()
{
    QMetaObject::invokeMethod(qApp, "exit", Qt::QueuedConnection, Q_ARG(int, -1));
}

void albert::App::quit()
{
    QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection);
}

void albert::App::registerExtension(albert::Extension *extension)
{
    if (!d->extensions.count(extension->id()))
        d->extensions.emplace(extension->id(), extension);
    emit extensionRegistered(extension);
}

void albert::App::unregisterExtension(albert::Extension *extension)
{
    if (auto it = d->extensions.find(extension->id()); it != d->extensions.end()){
        d->extensions.erase(it);
        emit extensionUnregistered(extension);
    } else
        qFatal("Unregistered unregistered extension: %s", qPrintable(extension->id()));
}

const std::map<QString, albert::Extension *> &albert::App::extensions()
{
    return d->extensions;
}


/*SOCKET*/
//QString App::handleSocketMessage(const QString &message)
//{
//    static std::map<QString, std::function<QString()>> actions = {
//            {"", [&](){
//                QStringList l;
//                for (const auto &[key, value] : actions)
//                    l << key;
//                return l.join(QChar::LineFeed);
//            }},
//            {"show", [&](){
//                native_plugin_provider.frontend().setVisible(true);
//                return "Frontend shown.";
//            }},
//            {"hide", [&](){
//                native_plugin_provider.frontend().setVisible(false);
//                return "Frontend hidden.";
//            }},
//            {"toggle", [&](){
//                native_plugin_provider.frontend().setVisible(!native_plugin_provider.frontend().isVisible());
//                return "Frontend toggled.";
//            }},
//            {"settings", [&](){
//                showSettings();
//                return "Settings opened,";
//            }},
//            {"docs", [&](){
//                albert::util::visitWebsite();
//                return "Opened website in a browser";
//            }},
//            {"restart", [&](){
//                restart();
//                return "Triggered restart.";
//            }},
//            {"quit", [&](){
//                quit();
//                return "quit shown";
//            }}
//    };
//
//    try{
//        return actions[message]();
//    } catch (const out_of_range&) {
//        WARN << QString("Received invalid RPC command: %1").arg(message);
//        return QString("Invalid RPC command: %1").arg(message);
//    }
//}

/*tray*/
//QAction* showAction = new QAction("Show", trayIconMenu.get());
//        trayIconMenu->addAction(showAction);
//
//        QAction* settingsAction = new QAction("Settings", trayIconMenu.get());
//        QObject::connect(settingsAction, &QAction::triggered, [&](){
//            settingsWidget->show();
//            settingsWidget->raise();
//        });
//        trayIconMenu->addAction(settingsAction);
//
//        QAction* docsAction = new QAction("Open docs", trayIconMenu.get());
//        QObject::connect(docsAction, &QAction::triggered, [](){
//            QDesktopServices::openUrl(QUrl("https://albertlauncher.github.io/"));
//        });
//        trayIconMenu->addAction(docsAction);
//
//        trayIconMenu->addSeparator();
//
//        QAction* restartAction = new QAction("Restart", trayIconMenu.get());
//        QObject::connect(restartAction, &QAction::triggered, [](){
//            QStringList cmdline(qApp->arguments());
//            if (QProcess::startDetached(cmdline.takeFirst(), cmdline)){
//                // TODO: Potential race conditions on slow systems
//                INFO << "Restarting application:" << cmdline;
//                qApp->quit();
//            } else {
//                WARN << "Restarting application failed:" << cmdline;
//            }
//        });
//        trayIconMenu->addAction(restartAction);
//
//        QAction* quitAction = new QAction("Quit", trayIconMenu.get());
//        QObject::connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
//        trayIconMenu->addAction(quitAction);
//
//        trayIcon->setContextMenu(trayIconMenu.get());

/*db*/
//        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
//        if ( !db.isValid() )
//            qFatal("No sqlite available");
//        if (!db.driver()->hasFeature(QSqlDriver::Transactions))
//            qFatal("QSqlDriver::Transactions not available.");
//        db.setDatabaseName(QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)).filePath("core.db"));
//        if (!db.open())
//            qFatal("Unable to establish a database connection.");
//
//        db.transaction();
//
//        QSqlQuery q(db);
//        if (!q.exec("CREATE TABLE IF NOT EXISTS query_handler ( "
//                    "  id INTEGER PRIMARY KEY NOT NULL, "
//                    "  string_id TEXT UNIQUE NOT NULL "
//                    "); "))
//            qFatal("Unable to create table 'query_handler': %s", q.lastError().text().toUtf8().constData());
//
//        if (!q.exec("CREATE TABLE IF NOT EXISTS query ( "
//                    "    id INTEGER PRIMARY KEY, "
//                    "    input TEXT NOT NULL, "
//                    "    cancelled INTEGER NOT NULL, "
//                    "    runtime INTEGER NOT NULL, "
//                    "    timestamp INTEGER DEFAULT CURRENT_TIMESTAMP "
//                    "); "))
//            qFatal("Unable to create table 'query': %s", q.lastError().text().toUtf8().constData());
//
//        if (!q.exec("CREATE TABLE IF NOT EXISTS execution ( "
//                    "    query_id INTEGER NOT NULL REFERENCES query(id) ON UPDATE CASCADE, "
//                    "    handler_id INTEGER NOT NULL REFERENCES query_handler(id) ON UPDATE CASCADE, "
//                    "    runtime INTEGER NOT NULL, "
//                    "    PRIMARY KEY (query_id, handler_id) "
//                    ") WITHOUT ROWID; "))
//            qFatal("Unable to create table 'execution': %s", q.lastError().text().toUtf8().constData());
//
//        if (!q.exec("CREATE TABLE IF NOT EXISTS activation ( "
//                    "    query_id INTEGER PRIMARY KEY NOT NULL REFERENCES query(id) ON UPDATE CASCADE, "
//                    "    item_id TEXT NOT NULL "
//                    "); "))
//            qFatal("Unable to create table 'activation': %s", q.lastError().text().toUtf8().constData());
//
//        if (!q.exec("CREATE TABLE IF NOT EXISTS conf(key TEXT UNIQUE, value TEXT); "))
//            qFatal("Unable to create table 'conf': %s", q.lastError().text().toUtf8().constData());
//
//        db.commit();

/*CoreQueryHandler*/
//struct CoreQueryHandler : public Core::QueryHandler
//{
//    CoreQueryHandler(const vector<shared_ptr<Item>>& items)
//        : Core::QueryHandler("org.albert"), items(items){}
//
//    void handleQuery(Core::Query * query) const override {
//        for (auto item : items)
//            if (!query->string().isEmpty() && item->text().toLower().startsWith(query->string().toLower()))
//                  query->addMatch(item, static_cast<uint>((query->string().length() / item->text().length()) * numeric_limits<uint>::max()));
//    }
//    vector<shared_ptr<Item>> items;
//};
////void App::addCoreItemProvider()
////{
//////    using namespace albert;
//////    struct CoreQueryHandler : public OfflineItemProvider {
//////        vector<IndexItem> index_items;
//////        QString id() const override { return "albert"; }
//////        vector<IndexItem> items() const override { return index_items;};
//////    };
//////
//////    auto *core_query_handler = new CoreQueryHandler;
//////
//////    core_query_handler->index_items = {
//////            {
//////                    makeSharedStandardItem(
//////                            "albert-preferences", QStringList{":app_icon"}, "Settings", "Open the Albert settings window",
//////                            SharedActionVector{makeFuncAction("Open settings.", [this] { showSettings(); })}
//////                    ),
//////                    {{"Settings",       MAX_SCORE}, {"Preferences", MAX_SCORE}}
//////            },
//////            {
//////                    makeSharedStandardItem(
//////                            "albert-quit", QStringList{":app_icon"}, "Quit Albert", "Quit this application",
//////                            SharedActionVector{makeFuncAction("Quit Albert.", [this] { quit(); })}
//////                    ),
//////                    {{"Quit Albert",    MAX_SCORE}}
//////            },
//////            {
//////                    makeSharedStandardItem(
//////                            "albert-restart", QStringList{":app_icon"}, "Restart Albert", "Restart this application",
//////                            SharedActionVector{makeFuncAction("Restart Albert.", [this] { restart(); })}
//////                    ),
//////                    {{"Restart Albert", MAX_SCORE}}
//////            }
//////    };
//////    registerExtension(core_query_handler);
////}        // Core items
//        coreQueryHandler = make_unique<CoreQueryHandler>(initializer_list<shared_ptr<Item>>{
//            makeStdItem("open-preferences",
//                ":app_icon", "Preferences", "Open the Albert preferences window.",
//                initializer_list<shared_ptr<Action>>{
//                    makeFuncAction("Open preferences.", [=](){ settingsAction->trigger(); })
//                }
//            ),
//            makeStdItem("restart-albert",
//                ":app_icon", "Restart Albert", "Restart this application.",
//                initializer_list<shared_ptr<Action>>{
//                    makeFuncAction("Restart Albert", [=](){ restartAction->trigger(); })
//                }
//            ),
//            makeStdItem("quit-albert",
//                ":app_icon", "Quit Albert", "Quit this application.",
//                initializer_list<shared_ptr<Action>>{
//                    makeFuncAction("Quit Albert", [=](){ quitAction->trigger(); })
//                }
//            )
//        });

/*hotkey*/
//std::unique_ptr<QHotkey> App::initializeHotkey(const QString &hotkey_overwrite)
//{
//    if (!hotkey_overwrite.isEmpty()) {
//        auto hk = make_unique<QHotkey>(QKeySequence(hotkey_overwrite), true, qApp);
//        if (!hk->isRegistered())
//            qFatal("Could not register hotkey overwrite '%s'", qPrintable(hotkey_overwrite));
//        return hk;
//    }
//
//    QSettings s(qApp->applicationName());
//    if (s.contains(CFG_HOTKEY)){
//        auto cfg_hotkey = s.value(CFG_HOTKEY).toString();
//        if (!cfg_hotkey.isEmpty()) {  // else hotkey is intended to be None
//            auto hk = make_unique<QHotkey>(QKeySequence(hotkey_overwrite), true, qApp);
//            if (!hk->isRegistered())
//                return hk;
//            else {
//                WARN << "Failed to register configured hotkey" << cfg_hotkey;
//                QMessageBox::warning(nullptr, "Warning",
//                                     "Failed to register the configured hotkey. Set a hotkey in the settings.");
//                showSettings();
//            }
//        }
//    } else {
//        auto hk = make_unique<QHotkey>(QKeySequence(DEF_HOTKEY), true, qApp);
//        if (hk->isRegistered())
//            return hk;
//        else {
//            WARN << "Failed to register default hotkey" << DEF_HOTKEY;
//            QMessageBox::warning(nullptr, "Warning",
//                                 "Failed to register the default hotkey. Configure a hotkey in the settings.");
//            showSettings();
//        }
//    }
//    return {};
//    //FIXME    if ( !QGuiApplication::platformName().contains("wayland") )
//    //        hotkeyManager = make_unique<HotkeyManager>();
//}
//

/*FRONTEND*/

//// Define a lambda that connects a new frontend
//auto connectFrontend = [&](Frontend *f){
//
//    QObject::connect(queryManager.get(), &QueryManager::resultsReady,
//                     f, &Frontend::setModel);
//
//    QObject::connect(showAction, &QAction::triggered,
//                     f, &Frontend::setVisible);
//
//    QObject::connect(trayIcon.get(), &TrayIcon::activated,
//                     f, [=](QSystemTrayIcon::ActivationReason reason){
//                if( reason == QSystemTrayIcon::ActivationReason::Trigger)
//                    f->toggleVisibility();
//            });
//
//    QObject::connect(f, &Frontend::settingsWidgetRequested, [&settingsWidget](){
//        settingsWidget->show();
//        settingsWidget->raise();
//        settingsWidget->activateWindow();
//    });
//
//    QObject::connect(f, &Frontend::widgetShown, [f, &queryManager](){
//        queryManager->setupSession();
//        queryManager->startQuery(f->input());
//    });
//
//    QObject::connect(f, &Frontend::widgetHidden,
//                     queryManager.get(), &QueryManager::teardownSession);
//
//    QObject::connect(f, &Frontend::inputChanged,
//                     queryManager.get(), &QueryManager::startQuery);
//};
//
//// Connect the current frontend
//connectFrontend(frontendManager->currentFrontend());
//
//// Connect new frontends
//QObject::connect(frontendManager.get(), &FrontendManager::frontendChanged, connectFrontend);
//}

//void PluginProvider::setFrontend(const QString &id)
//{
//    if (!plugin_index.count(id))
//        qFatal("Set an invalid frontend id: %s", qPrintable(id));
//    QSettings(qApp->applicationName()).setValue(CFG_FRONTEND_ID, id);
//}

//void PluginProvider::loadFrontend()
//{
//    using albert::PluginSpec;
//    using albert::Frontend;
//
//    auto frontend_plugins = frontendPlugins();
//    if (frontend_plugins.empty())
//        qFatal("No frontends available");
//
//    auto load_frontend = [this](const QString &id) -> bool {
//        try {
//            if (frontend_spec_ = plugin_index.at(id); !frontend_spec_->load())
//                WARN << "Frontend failed loading:" << id << frontend_spec_->reason();
//            else if (frontend_ = dynamic_cast<Frontend *>(frontend_spec_->instance()); !frontend_)
//                WARN << "Frontend is not of type 'Frontend':" << id;
//            else{
//                // Auto registration of root extensions
//                if (albert::Extension *extension = dynamic_cast<albert::Extension*>(frontend_))
//                    extension_registry.registerExtension(extension);
//                return true;
//            }
//        } catch (const out_of_range &e) {
//            WARN << "Frontend does not exist:" << id;
//        }
//        return false;
//    };
//
//    if (auto id = QSettings(qApp->applicationName()).value(CFG_FRONTEND_ID).toString(); !id.isNull()){
//        if (load_frontend(id))
//            return;
//        else
//            CRIT << "Loading configured frontend failed:" << id;
//    }
//
//    if (load_frontend(DEF_FRONTEND_ID))
//        return;
//    else
//        CRIT << "Loading default frontend failed:" << DEF_FRONTEND_ID;
//
//    CRIT << "Try to load any fallback frontend.";
//
//    for (const auto &[fbid, frontend_plugin] : frontend_plugins){
//        if (load_frontend(fbid))
//            return;
//        else
//            CRIT << "Loading fallback frontend failed:" << fbid;
//    }
//
//    qFatal("All frontends failed to load!");
//}

/*extension registry auto*/

//#include <set>
//#include "extension.h"
//#include "extensionmanager.h"
//using namespace std;
//
//
//Core::ExtensionManager *Core::Extension::extensionManager = nullptr;
//
//struct Core::Private {
//    set<QueryHandler*> registeredQueryHandlers;
//    set<FallbackProvider*> registeredFallbackProviders;
//};
//
//
///**************************************************************************************/
//Core::Extension::Extension(const QString &id) : Plugin(id), d(new Private) {
//
//}
//
//
///**************************************************************************************************/
//Core::Extension::~Extension() {
//    // If the extensin did it not by itself unregister all the remaining handlers
//    for (auto ptr : d->registeredQueryHandlers)
//        unregisterQueryHandler(ptr);
//    for (auto ptr : d->registeredFallbackProviders)
//        unregisterFallbackProvider(ptr);
//}
//
//
///**************************************************************************************************/
//void Core::Extension::registerQueryHandler(Core::QueryHandler *object) {
//    d->registeredQueryHandlers.insert(object);
//    extensionManager->registerQueryHandler(object);
//}
//
//
///**************************************************************************************************/
//void Core::Extension::unregisterQueryHandler(Core::QueryHandler *object) {
//    extensionManager->unregisterQueryHandler(object);
//}
//
//
///**************************************************************************************************/
//void Core::Extension::registerFallbackProvider(Core::FallbackProvider *object) {
//    d->registeredFallbackProviders.insert(object);
//    extensionManager->registerFallbackProvider(object);
//}
//
//
///**************************************************************************************************/
//void Core::Extension::unregisterFallbackProvider(Core::FallbackProvider *object) {
//    extensionManager->unregisterFallbackProvider(object);
//}