#pragma once

#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>

class StreamElementsMenuManager
{
public:
	StreamElementsMenuManager(QMainWindow* parent);
	virtual ~StreamElementsMenuManager();

public:
	void Update();

protected:
	QMainWindow* mainWindow() { return m_mainWindow; }

private:
	QMainWindow* m_mainWindow;
	QMenu* m_menu;
};
