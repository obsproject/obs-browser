#ifndef STREAMELEMENTSPROGRESSDIALOG_HPP
#define STREAMELEMENTSPROGRESSDIALOG_HPP

#include <QDialog>

namespace Ui {
class StreamElementsProgressDialog;
}

class StreamElementsProgressDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StreamElementsProgressDialog(QWidget *parent = 0);
    ~StreamElementsProgressDialog();

public:
	virtual void accept();
	virtual void reject();

	bool cancelled() { return m_cancelled; }

public slots:
	void setEnableCancel(bool enable);
	void setMessage(std::string msg);
	void setProgress(int min, int max, int value);

private:
	bool m_cancelled = false;

private:
    Ui::StreamElementsProgressDialog *ui;
};

#endif // STREAMELEMENTSPROGRESSDIALOG_HPP
