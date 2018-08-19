#include "StreamElementsProgressDialog.hpp"
#include "ui_StreamElementsProgressDialog.h"

StreamElementsProgressDialog::StreamElementsProgressDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::StreamElementsProgressDialog)
{
    ui->setupUi(this);
}

StreamElementsProgressDialog::~StreamElementsProgressDialog()
{
    delete ui;
}

void StreamElementsProgressDialog::accept()
{
	QDialog::accept();
}

void StreamElementsProgressDialog::reject()
{
	m_cancelled = true;

	QDialog::reject();
}

void StreamElementsProgressDialog::setEnableCancel(bool enable)
{
	QMetaObject::invokeMethod(ui->cmdCancel, "setEnabled", Qt::QueuedConnection, Q_ARG(bool, enable));
}

void StreamElementsProgressDialog::setMessage(std::string msg)
{
	QMetaObject::invokeMethod(ui->txtOp, "setText", Qt::QueuedConnection, Q_ARG(QString, QString(msg.c_str())));
}

void StreamElementsProgressDialog::setProgress(int min, int max, int value)
{
	if (ui->progressBar->minimum() != min) {
		QMetaObject::invokeMethod(ui->progressBar, "setMinimum", Qt::QueuedConnection, Q_ARG(int, min));
	}

	if (ui->progressBar->maximum() != max) {
		QMetaObject::invokeMethod(ui->progressBar, "setMaximum", Qt::QueuedConnection, Q_ARG(int, max));
	}

	if (ui->progressBar->value() != value) {
		QMetaObject::invokeMethod(ui->progressBar, "setValue", Qt::QueuedConnection, Q_ARG(int, value));
	}
}
