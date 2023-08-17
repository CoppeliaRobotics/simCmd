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

signals:
    void execCode(QString code, int scriptType, QString scriptName);
    void askCompletion(int scriptHandleOrType, QString scriptName, QString token, QChar context, QStringList *cl);

private:
    Replxx rx;
};
