// albert - a simple application launcher for linux
// Copyright (C) 2014 Manuel Schneider
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef ENGINE_H
#define ENGINE_H

#include "abstractservice.h"
#include <QVector>
#include <QSettings>
#include <QAbstractListModel>

class Engine : public QAbstractListModel
{
	Q_OBJECT
	friend class SettingsWidget;

public:
	Engine();
	~Engine();

	int      rowCount(const QModelIndex & = QModelIndex()) const override;
	QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;

	void initialize();
	void saveSettings(QSettings &s) const;
	void loadSettings(QSettings &s);
	void serilizeData(QDataStream &out) const;
	void deserilizeData(QDataStream &in);


private:
	QVector<Service*>        _modules;
	QVector<Service::Item *> _data;
	QString                  _requestString;

public slots:
	void query(const QString &req);
};

#endif // ENGINE_H
