#ifndef SAVELOAD_H
#define SAVELOAD_H

#include <QDialog>

namespace Ui {
class SaveLoad;
}

class SaveLoad : public QDialog
{
    Q_OBJECT

public:
    explicit SaveLoad(QWidget *parent = nullptr);
    ~SaveLoad();

private slots:

    void on_Save_clicked();

    void on_Load_clicked();

signals:

    void Save_sig(std::string name);

    void Load_sig(std::string name);


private:

    Ui::SaveLoad *ui;
};

#endif // SAVELOAD_H
