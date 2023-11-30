#pragma once

#include <QThread>
#include <QStringList>

#include <replxx.hxx>

using Replxx = replxx::Replxx;

class Readline : public QThread
{
    Q_OBJECT

public:
    Readline(QObject *parent);
    void run() override;
    Replxx::completions_t hook_completion(const std::string &context, int &contextLen);

public slots:
    void setSelectedScript(int scriptHandle, QString lang);

signals:
    void execCode(int scriptHandle, QString langSuffix, QString code);
    void askCompletion(int scriptHandle, QString langSuffix, QString input, int pos, QStringList *cl);

private:
    Replxx rx;
    int sandboxScript;
    QString preferredSandboxLang;
    int scriptHandle;
    QString lang;
    bool havePython;
};
