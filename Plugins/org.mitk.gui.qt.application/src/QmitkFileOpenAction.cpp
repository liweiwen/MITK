/*===================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center,
Division of Medical and Biological Informatics.
All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt or http://www.mitk.org for details.

===================================================================*/

#include "QmitkFileOpenAction.h"

#include "internal/org_mitk_gui_qt_application_Activator.h"

#include <mitkWorkbenchUtil.h>

#include <QmitkIOUtil.h>

#include <berryIPreferences.h>

#include <QFileDialog>

class QmitkFileOpenActionPrivate
{
public:

  void Init(berry::IWorkbenchWindow* window, QmitkFileOpenAction* action)
  {
    m_Window = window;
    action->setText("&Open File...");
    action->setToolTip("Open data files (images, surfaces,...)");

    QObject::connect(action, SIGNAL(triggered(bool)), action, SLOT(Run()));
  }

  berry::IPreferences::Pointer GetPreferences() const
  {
    berry::IPreferencesService* prefService = mitk::PluginActivator::GetInstance()->GetPreferencesService();
    if (prefService != nullptr)
    {
      return prefService->GetSystemPreferences()->Node("/General");
    }
    return berry::IPreferences::Pointer(nullptr);
  }

  QString GetLastFileOpenPath() const
  {
    berry::IPreferences::Pointer prefs = GetPreferences();
    if (prefs.IsNotNull())
    {
      return prefs->Get("LastFileOpenPath", "");
    }
    return QString();
  }

  void SetLastFileOpenPath(const QString& path) const
  {
    berry::IPreferences::Pointer prefs = GetPreferences();
    if (prefs.IsNotNull())
    {
      prefs->Put("LastFileOpenPath", path);
      prefs->Flush();
    }
  }

  bool GetOpenEditor() const
  {
    berry::IPreferences::Pointer prefs = GetPreferences();
    if (prefs.IsNotNull())
    {
      return prefs->GetBool("OpenEditor", true);
    }
    return true;
  }

  berry::IWorkbenchWindow* m_Window;
};

QmitkFileOpenAction::QmitkFileOpenAction(berry::IWorkbenchWindow::Pointer window)
  : QAction(nullptr)
  , d(new QmitkFileOpenActionPrivate)
{
  d->Init(window.GetPointer(), this);
}

QmitkFileOpenAction::QmitkFileOpenAction(const QIcon& icon, berry::IWorkbenchWindow::Pointer window)
  : QAction(nullptr)
  , d(new QmitkFileOpenActionPrivate)
{
  d->Init(window.GetPointer(), this);
  setIcon(icon);
}

QmitkFileOpenAction::QmitkFileOpenAction(const QIcon& icon, berry::IWorkbenchWindow* window)
  : QAction(nullptr), d(new QmitkFileOpenActionPrivate)
{
  d->Init(window, this);
  setIcon(icon);
}

QmitkFileOpenAction::~QmitkFileOpenAction()
{
}

void QmitkFileOpenAction::Run()
{
  // ask the user for a list of files to open
  QStringList fileNames = QFileDialog::getOpenFileNames(nullptr, "Open",
                                                        d->GetLastFileOpenPath(),
                                                        QmitkIOUtil::GetFileOpenFilterString());

  if (fileNames.empty())
  {
    return;
  }

  d->SetLastFileOpenPath(fileNames.front());
  mitk::WorkbenchUtil::LoadFiles(fileNames, berry::IWorkbenchWindow::Pointer(d->m_Window), d->GetOpenEditor());
}
