#include "scanner.h"

#include <QtDebug>

Scanner::Scanner()
{
    script = nullptr;
    currentId = 0;
}

void Scanner::setFile(QString fileName)
{
    script = new Script(fileName,-1);
    script->moveToThread(QApplication::instance()->thread());
}

void Scanner::setRecursive(bool r)
{
    recursive = r;
}

void Scanner::run()
{
    if ( scan(script) ) emit finished(script);
    else emit finished(nullptr);
}

bool Scanner::scan(Script *s)
{
    if (s == nullptr) return false;


#ifdef QT_DEBUG
    qDebug() << "Scanning ===== " + s->name();
#endif

    QFile *scriptFile = s->file();
    emit progress(-1,"Scanning \"" + s->name() + "\"");
    currentId++;
    s->setId(currentId);

    //Check script file integrity
    if (!scriptFile->exists())
    {
        emit openFailed();
        return false;
    }
    //open file
    if (!scriptFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        emit openFailed();
        return false;
    }

    //list of include paths (if found)
    QStringList includePaths;

    //get includes
    int lineNumber = 0;
    QRegularExpression reInclude("^(?!.*\\/\\/)(?!.*\\/\\*).*#include +([\"']?)([^\"'\\r\\n\\t]+)\\1 *;?$");
    QRegularExpression reIncludePath("^(?!.*\\/\\/)(?!.*\\/\\*).*#includepath +([\"']?)([^\"'\\r\\n\\t]+)\\1 *;?$");

    bool comment = false;

    while (!scriptFile->atEnd())
    {
        QString line = scriptFile->readLine();
        lineNumber++;

        if (comment)
        {
            if (line.contains("*/")) comment = false;
        }

        if (comment) continue;

        QRegularExpressionMatch matchInclude = reInclude.match(line);
        if (matchInclude.hasMatch())
        {
            currentId++;
            QString name = matchInclude.captured(2);
            QString path = checkIncludePath(name,includePaths,s);
            Script *includedScript = new Script(name,path,lineNumber);
            includedScript->setId(currentId);
            s->addInclude(includedScript);
            //if recursive
            if (recursive && includedScript->exists())
            {
                scan(includedScript);
            }
            continue;
        }

        if (line.contains("/*")) comment = true;
    }

    //close file
    scriptFile->close();

    return true;
}

QString Scanner::checkIncludePath(QString name, QStringList includePaths,Script *s)
{
    QFile *scriptFile = s->file();
    QString scriptPath = QFileInfo(*scriptFile).absolutePath();


    //#include
    QRegularExpression reAbsolutePathWin("^[a-z]{1}:");
    reAbsolutePathWin.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    QRegularExpression reAbsolutePathMac("^/");

    //absolute
    if (reAbsolutePathWin.match(name).hasMatch() || reAbsolutePathMac.match(name).hasMatch() )
    {
        return name;
    }

    //relative

    //test script path
    QString path = addPathSlash(scriptPath) + name;
    if (QFile(path).exists()) return path;

    //test includes
    foreach(QString includePath,includePaths)
    {
        path = addPathSlash(includePath) + name;
        if (QFile(path).exists()) return path;
    }

    if (QFile(name).exists()) return name;

    //test default include paths
    int numIncludePaths = settings.beginReadArray("includePaths");
    for (int i = 0; i < numIncludePaths; i++)
    {
        settings.setArrayIndex(i);
        path = addPathSlash(settings.value("path").toString()) + name;
        if (QFile(path).exists())
        {
            settings.endArray();
            return path;
        }
    }
    settings.endArray();

    //test again default include paths but stripping path from given name
    settings.beginReadArray("includePaths");
    for (int i = 0; i < numIncludePaths; i++)
    {
        settings.setArrayIndex(i);
        path = addPathSlash(settings.value("path").toString()) + QFileInfo(name).fileName();
        qDebug() << path;
        if (QFile(path).exists())
        {
            settings.endArray();
            return path;
        }
    }
    settings.endArray();

    return scriptPath + "/" + name;
}

QString Scanner::addPathSlash(QString path)
{
    if (!path.endsWith("/") || path.endsWith("\\")) path += "/";
    return path;
}


