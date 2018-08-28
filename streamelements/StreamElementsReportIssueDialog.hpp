#pragma once

#ifndef STREAMELEMENTSREPORTISSUEDIALOG_H
#define STREAMELEMENTSREPORTISSUEDIALOG_H

#include <QDialog>

namespace Ui {
class StreamElementsReportIssueDialog;
}

class StreamElementsReportIssueDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StreamElementsReportIssueDialog(QWidget *parent = 0);
    ~StreamElementsReportIssueDialog();

public slots:
	virtual void update();
	virtual void accept();

private:
    Ui::StreamElementsReportIssueDialog *ui;
};

#endif // STREAMELEMENTSREPORTISSUEDIALOG_H
