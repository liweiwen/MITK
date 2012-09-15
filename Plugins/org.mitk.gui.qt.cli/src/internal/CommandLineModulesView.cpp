/*===================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) University College London (UCL).
All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt or http://www.mitk.org for details.

===================================================================*/


// Blueberry
#include <berryIPreferences.h>
#include <berryIPreferencesService.h>
#include <berryIBerryPreferences.h>

// Qmitk
#include "CommandLineModulesView.h"
#include "CommandLineModulesViewConstants.h"
#include "CommandLineModulesViewControls.h"
#include "CommandLineModulesPreferencesPage.h"
#include "QmitkCmdLineModuleFactoryGui.h"
#include "QmitkCmdLineModuleGui.h"
#include "QmitkCmdLineModuleProgressWidget.h"

// Qt
#include <QDebug>
#include <QDir>
#include <QAction>
#include <QVBoxLayout>
#include <QLayoutItem>
#include <QWidgetItem>
#include <QMessageBox>

// CTK
#include <ctkCmdLineModuleManager.h>
#include <ctkCmdLineModuleFrontend.h>
#include <ctkCmdLineModuleBackendLocalProcess.h>
#include <ctkCmdLineModuleDefaultPathBuilder.h>
#include <ctkCmdLineModuleDirectoryWatcher.h>
#include <ctkCmdLineModuleReference.h>
#include <ctkCmdLineModuleDescription.h>

//-----------------------------------------------------------------------------
CommandLineModulesView::CommandLineModulesView()
: m_Controls(NULL)
, m_Parent(NULL)
, m_Layout(NULL)
, m_ModuleManager(NULL)
, m_ModuleBackend(NULL)
, m_DirectoryWatcher(NULL)
, m_TemporaryDirectoryName("")
, m_MaximumConcurrentProcesses(4)
, m_CurrentlyRunningProcesses(0)
, m_DebugOutput(false)
{
}


//-----------------------------------------------------------------------------
CommandLineModulesView::~CommandLineModulesView()
{
  if (m_ModuleManager != NULL)
  {
    delete m_ModuleManager;
  }

  if (m_ModuleBackend != NULL)
  {
    delete m_ModuleBackend;
  }

  if (m_DirectoryWatcher != NULL)
  {
    delete m_DirectoryWatcher;
  }

  if (m_Layout != NULL)
  {
    delete m_Layout;
  }

  for (int i = 0; i < m_ListOfModules.size(); i++)
  {
    delete m_ListOfModules[i];
  }
}


//-----------------------------------------------------------------------------
void CommandLineModulesView::SetFocus()
{
  this->m_Controls->m_ComboBox->setFocus();
}


//-----------------------------------------------------------------------------
void CommandLineModulesView::CreateQtPartControl( QWidget *parent )
{
  m_Parent = parent;

  if (!m_Controls)
  {
    // We create CommandLineModulesViewControls, which derives from the Qt generated class.
    m_Controls = new CommandLineModulesViewControls(parent);

    // Create a layout to contain a display of QmitkCmdLineModuleProgressWidget.
    m_Layout = new QVBoxLayout(m_Controls->m_RunningWidgets);
    m_Layout->setContentsMargins(0,0,0,0);
    m_Layout->setSpacing(0);

    // This must be done independent of other preferences, as we need it before
    // we create the ctkCmdLineModuleManager to initialise the Cache.
    this->RetrieveAndStoreTemporaryDirectoryPreferenceValues();
    this->RetrieveAndStoreValidationMode();

    // Start to create the command line module infrastructure.
    m_ModuleBackend = new ctkCmdLineModuleBackendLocalProcess();
    m_ModuleManager = new ctkCmdLineModuleManager(m_ValidationMode, m_TemporaryDirectoryName);

    // Set the main object, the ctkCmdLineModuleManager onto all the objects that need it.
    m_Controls->m_ComboBox->SetManager(m_ModuleManager);
    m_DirectoryWatcher = new ctkCmdLineModuleDirectoryWatcher(m_ModuleManager);
    m_ModuleManager->registerBackend(m_ModuleBackend);

    // Setup the remaining preferences.
    this->RetrieveAndStorePreferenceValues();

    // Connect signals to slots after we have set up GUI.
    connect(this->m_Controls->m_RunButton, SIGNAL(pressed()), this, SLOT(OnRunButtonPressed()));
    connect(this->m_Controls->m_RestoreDefaults, SIGNAL(pressed()), this, SLOT(OnRestoreButtonPressed()));
    connect(this->m_Controls->m_ComboBox, SIGNAL(actionChanged(QAction*)), this, SLOT(OnActionChanged(QAction*)));
    connect(this->m_Controls->m_TabWidget, SIGNAL(tabCloseRequested(int)), this, SLOT(OnTabCloseRequested(int)));

    this->UpdateRunButtonEnabledStatus();
  }
}


//-----------------------------------------------------------------------------
berry::IBerryPreferences::Pointer CommandLineModulesView::RetrievePreferences()
{
  berry::IPreferencesService::Pointer prefService
      = berry::Platform::GetServiceRegistry()
      .GetServiceById<berry::IPreferencesService>(berry::IPreferencesService::ID);

  assert( prefService );

  std::string id = "/" + CommandLineModulesViewConstants::VIEW_ID;
  berry::IBerryPreferences::Pointer prefs
      = (prefService->GetSystemPreferences()->Node(id))
        .Cast<berry::IBerryPreferences>();

  assert( prefs );

  return prefs;
}


//-----------------------------------------------------------------------------
void CommandLineModulesView::RetrieveAndStoreTemporaryDirectoryPreferenceValues()
{
  berry::IBerryPreferences::Pointer prefs = this->RetrievePreferences();

  QString fallbackTmpDir = QDir::tempPath();
  m_TemporaryDirectoryName = QString::fromStdString(
      prefs->Get(CommandLineModulesViewConstants::TEMPORARY_DIRECTORY_NODE_NAME, fallbackTmpDir.toStdString()));
}


//-----------------------------------------------------------------------------
void CommandLineModulesView::RetrieveAndStoreValidationMode()
{
  berry::IBerryPreferences::Pointer prefs = this->RetrievePreferences();

  int value = prefs->GetInt(CommandLineModulesViewConstants::XML_VALIDATION_MODE, 0);
  if (value == 0)
  {
    m_ValidationMode = ctkCmdLineModuleManager::STRICT_VALIDATION;
  }
  else if (value == 1)
  {
    m_ValidationMode = ctkCmdLineModuleManager::SKIP_VALIDATION;
  }
  else
  {
    m_ValidationMode = ctkCmdLineModuleManager::WEAK_VALIDATION;
  }
}


//-----------------------------------------------------------------------------
void CommandLineModulesView::RetrieveAndStorePreferenceValues()
{
  berry::IBerryPreferences::Pointer prefs = this->RetrievePreferences();

  m_MaximumConcurrentProcesses = prefs->GetInt(CommandLineModulesViewConstants::MAX_CONCURRENT, 4);

  // Get the flag for debug output, useful when parsing all the XML.
  m_DebugOutput = prefs->GetBool(CommandLineModulesViewConstants::DEBUG_OUTPUT_NODE_NAME, false);
  m_DirectoryWatcher->setDebug(m_DebugOutput);

  bool loadApplicationDir = prefs->GetBool(CommandLineModulesViewConstants::LOAD_FROM_APPLICATION_DIR, false);
  bool loadHomeDir = prefs->GetBool(CommandLineModulesViewConstants::LOAD_FROM_HOME_DIR, false);
  bool loadCurrentDir = prefs->GetBool(CommandLineModulesViewConstants::LOAD_FROM_CURRENT_DIR, false);
  bool loadAutoLoadDir = prefs->GetBool(CommandLineModulesViewConstants::LOAD_FROM_AUTO_LOAD_DIR, false);

  // Get some default application paths.

  // Here we can use the preferences to set up the builder,
  ctkCmdLineModuleDefaultPathBuilder builder;
  builder.setLoadFromApplicationDir(loadApplicationDir);
  builder.setLoadFromHomeDir(loadHomeDir);
  builder.setLoadFromCurrentDir(loadCurrentDir);
  builder.setLoadFromCtkModuleLoadPath(loadAutoLoadDir);

  // and then we ask the builder to set up the paths.
  QStringList defaultPaths = builder.build();

  // We get additional directory paths from preferences.
  QString pathString = QString::fromStdString(prefs->Get(CommandLineModulesViewConstants::MODULE_DIRECTORIES_NODE_NAME, ""));
  QStringList additionalPaths = pathString.split(";", QString::SkipEmptyParts);

  // Combine the sets of directory paths.
  QStringList totalPaths;
  totalPaths << defaultPaths;
  totalPaths << additionalPaths;

  QString additionalModulesString = QString::fromStdString(prefs->Get(CommandLineModulesViewConstants::MODULE_FILES_NODE_NAME, ""));
  QStringList additionalModules = additionalModulesString.split(";", QString::SkipEmptyParts);

  // OnPreferencesChanged can be called for each preference in a dialog box, so
  // when you hit "OK", it is called repeatedly, whereas we want to only call these only once.
  if (this->m_DirectoryWatcher->directories() != totalPaths)
  {
    m_DirectoryWatcher->setDirectories(totalPaths);
  }
  if (this->m_DirectoryWatcher->additionalModules() != additionalModules)
  {
    m_DirectoryWatcher->setAdditionalModules(additionalModules);
  }
}


//-----------------------------------------------------------------------------
void CommandLineModulesView::OnPreferencesChanged(const berry::IBerryPreferences* /*prefs*/)
{
  this->RetrieveAndStoreTemporaryDirectoryPreferenceValues();
  this->RetrieveAndStoreValidationMode();
  this->RetrieveAndStorePreferenceValues();
}


//-----------------------------------------------------------------------------
ctkCmdLineModuleReference CommandLineModulesView::GetReferenceByFullName(QString fullName)
{
  ctkCmdLineModuleReference result;

  QList<ctkCmdLineModuleReference> references = this->m_ModuleManager->moduleReferences();

  ctkCmdLineModuleReference ref;
  foreach(ref, references)
  {
    QString name = ref.description().categoryDotTitle();
    if (name == fullName)
    {
      result = ref;
    }
  }

  return result;
}


//-----------------------------------------------------------------------------
void CommandLineModulesView::OnActionChanged(QAction* action)
{
  QString fullName = action->objectName();
  ctkCmdLineModuleReference ref = this->GetReferenceByFullName(fullName);

  // ref should never be invalid, as the menu was generated from each ctkCmdLineModuleReference.
  // But just to be sure ... if invalid, don't do anything.
  if (ref)
  {
    // Check if we already have the reference.
    int tabIndex = -1;
    for (int i = 0; i < m_ListOfModules.size(); i++)
    {
      ctkCmdLineModuleReference tabsReference = m_ListOfModules[i]->moduleReference();
      if (ref.location() == tabsReference.location())
      {
        tabIndex = i;
        break;
      }
    }

    // i.e. we found a matching tab, so just switch to it.
    if (tabIndex != -1)
    {
      m_Controls->m_TabWidget->setCurrentIndex(tabIndex);
    }
    else // i.e. we did not find a matching tab
    {
      // In this case, we need to create a new tab, and give
      // it a GUI for the user to setup the parameters with.
      QmitkCmdLineModuleFactoryGui factory(this->GetDataStorage());
      ctkCmdLineModuleFrontend *frontEnd = factory.create(ref);
      QmitkCmdLineModuleGui *theGui = dynamic_cast<QmitkCmdLineModuleGui*>(frontEnd);

      // Add to list and tab wigdget
      m_ListOfModules.push_back(frontEnd);
      int tabIndex = m_Controls->m_TabWidget->addTab(theGui->getGui(), ref.description().title());
      m_Controls->m_TabWidget->setTabToolTip(tabIndex, ref.description().title() + ":" + ref.xmlValidationErrorString());
    }
  }
}


//-----------------------------------------------------------------------------
void CommandLineModulesView::OnTabCloseRequested(int tabNumber)
{

  ctkCmdLineModuleFrontend *frontEnd = m_ListOfModules[tabNumber];

  m_Controls->m_TabWidget->removeTab(tabNumber);
  m_ListOfModules.removeAt(tabNumber);

  delete frontEnd;
}


//-----------------------------------------------------------------------------
void CommandLineModulesView::AskUserToSelectAModule() const
{
  QMessageBox msgBox;
  msgBox.setText("Please select a module!");
  msgBox.setIcon(QMessageBox::Warning);
  msgBox.exec();
}


//-----------------------------------------------------------------------------
void CommandLineModulesView::OnRestoreButtonPressed()
{
  int tabNumber = m_Controls->m_TabWidget->currentIndex();
  if (tabNumber >= 0)
  {
    ctkCmdLineModuleFrontend *frontEnd = m_ListOfModules[tabNumber];
    frontEnd->resetValues();
  }
  else
  {
    this->AskUserToSelectAModule();
  }
}


//-----------------------------------------------------------------------------
void CommandLineModulesView::OnRunButtonPressed()
{
  int tabNumber = m_Controls->m_TabWidget->currentIndex();
  if (tabNumber >= 0)
  {
    // 1. Create a new QmitkCmdLineModuleProgressWidget to represent the running widget.
    QmitkCmdLineModuleProgressWidget *widget = new QmitkCmdLineModuleProgressWidget(m_Controls->m_RunningWidgets);
    widget->SetDataStorage(this->GetDataStorage());
    widget->SetManager(m_ModuleManager);
    widget->SetTemporaryDirectory(m_TemporaryDirectoryName);

    // 2. Create a new front end.
    QmitkCmdLineModuleFactoryGui factory(this->GetDataStorage());

    ctkCmdLineModuleFrontend *frontEndOnCurrentTab = m_ListOfModules[tabNumber];
    QmitkCmdLineModuleGui *frontEndGuiOnCurrentTab = dynamic_cast<QmitkCmdLineModuleGui*>(frontEndOnCurrentTab);
    ctkCmdLineModuleReference currentTabFrontendReferences = frontEndGuiOnCurrentTab->moduleReference();

    ctkCmdLineModuleFrontend *newFrontEnd = factory.create(currentTabFrontendReferences);
    QmitkCmdLineModuleGui *newFrontEndGui = dynamic_cast<QmitkCmdLineModuleGui*>(newFrontEnd);
    widget->SetFrontend(newFrontEndGui);
    m_Layout->insertWidget(0, widget);

    // 3. Copy parameters. This MUST come after widget->SetFrontEnd
    newFrontEndGui->copyParameters(*frontEndGuiOnCurrentTab);
    newFrontEndGui->setParametersEnabled(false);

    // 4. Connect widget signals to here, to count how many jobs running.
    connect(widget, SIGNAL(started()), this, SLOT(OnJobStarted()));
    connect(widget, SIGNAL(finished()), this, SLOT(OnJobFinished()));

    // 5. GO.
    widget->Run();
  }
  else
  {
    this->AskUserToSelectAModule();
  }
}


//-----------------------------------------------------------------------------
void CommandLineModulesView::UpdateRunButtonEnabledStatus()
{
  if (m_CurrentlyRunningProcesses >= m_MaximumConcurrentProcesses)
  {
    m_Controls->m_RunButton->setEnabled(false);
  }
  else
  {
    m_Controls->m_RunButton->setEnabled(true);
  }
}


//-----------------------------------------------------------------------------
void CommandLineModulesView::OnJobStarted()
{
  m_CurrentlyRunningProcesses++;
  this->UpdateRunButtonEnabledStatus();
}


//-----------------------------------------------------------------------------
void CommandLineModulesView::OnJobFinished()
{
  m_CurrentlyRunningProcesses--;
  this->UpdateRunButtonEnabledStatus();
}
