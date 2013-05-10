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


#include "mitkFileReaderManager.h"

// Microservices
#include <usGetModuleContext.h>
#include <mitkModuleContext.h>
#include <usServiceProperties.h>


//////////////////// READING DIRECTLY ////////////////////

mitk::BaseData::Pointer mitk::FileReaderManager::Read(const std::string& path)
{
  mitk::ModuleContext * context = mitk::GetModuleContext();
  // Find extension
  std::string extension = path;
  extension.erase(0, path.find_last_of('.'));

  // Get best Reader
  mitk::IFileReader* reader = GetReader(extension);
  // Throw exception if no compatible reader was found
  if (reader == 0) mitkThrow() << "Tried to directly read a file of type '" + extension + "' via FileReaderManager, but no reader supporting this filetype was found.";
  itk::SmartPointer<mitk::BaseData> result = reader->Read(path);
  // TODO evil HotFix: We are loosing the smartpointer reference here otherwise...
  result->Register();
  return result;
}


//////////////////// GETTING READERS ////////////////////

mitk::IFileReader* mitk::FileReaderManager::GetReader(const std::string& extension, mitk::ModuleContext* context )
{
  std::list <mitk::ServiceReference> results = GetReaderList(extension, context);
  if (results.size() == 0) return 0;
  return context->GetService<mitk::IFileReader>(results.front());
}

std::list <mitk::IFileReader*> mitk::FileReaderManager::GetReaders(const std::string& extension, mitk::ModuleContext* context )
{
  std::list <mitk::IFileReader*> result;
  std::list <mitk::ServiceReference > refs = GetReaderList(extension, context);

  // Translate List of ServiceRefs to List of Pointers
  while ( !refs.empty() )
  {
    result.push_back( context->GetService<mitk::IFileReader>(refs.front()));
    refs.pop_front();
  }

  return result;
}

mitk::IFileReader* mitk::FileReaderManager::GetReader(const std::string& extension, const std::list<std::string>& options, mitk::ModuleContext* context )
{
  std::list <mitk::IFileReader*> matching = mitk::FileReaderManager::GetReaders(extension, options, context);
  if (matching.size() == 0) return 0;
  return matching.front();
}

std::list <mitk::IFileReader*> mitk::FileReaderManager::GetReaders(const std::string& extension, const std::list<std::string>& options, mitk::ModuleContext* context )
{
  std::list <mitk::IFileReader*> allReaders = mitk::FileReaderManager::GetReaders(extension, context);
  std::list <mitk::IFileReader*> result;
  // the list is already sorted by priority. Now find reader that supports all options

  while (allReaders.size() > 0)
  {
    mitk::IFileReader * currentReader = allReaders.front();
    allReaders.pop_front();
    // Now see if this reader supports all options. If yes, push to results
    if ( mitk::FileReaderManager::ReaderSupportsOptions(currentReader, options) ) result.push_back(currentReader);
  }
  return result;
}


//////////////////// INTERNAL CODE ////////////////////

bool mitk::FileReaderManager::ReaderSupportsOptions(mitk::IFileReader* reader, std::list<std::string> options )
{
  std::list<std::string> readerOptions = reader->GetSupportedOptions();
  if (options.size() == 0) return true;         // if no options were requested, return true unconditionally
  if (readerOptions.size() == 0) return false;  // if options were requested and reader supports no options, return false

  // For each of the strings in requuested options, check if option is available in reader
  for(std::list<std::string>::iterator options_i = options.begin(); options_i != options.end(); options_i++)
  {
    {
      bool optionFound = false;
      // Iterate over each available option from reader to check if one of them matches the current option
      for(std::list<std::string>::iterator options_j = readerOptions.begin(); options_j != readerOptions.end(); options_j++)
        if ( *options_i == *options_j ) optionFound = true;
      if (optionFound == false) return false; // If one option was not found, leave method and return false
    }
  }
  return true; // if all options have been found, return true
}



//////////////////// uS-INTERACTION ////////////////////

std::list< mitk::ServiceReference > mitk::FileReaderManager::GetReaderList(const std::string& extension, mitk::ModuleContext* context )
{
  // filter for class and extension
  std::string filter = "(&(" + mitk::ServiceConstants::OBJECTCLASS() + "=org.mitk.services.FileReader)(" + mitk::IFileReader::PROP_EXTENSION + "=" + extension + "))";
  std::list <mitk::ServiceReference> result = context->GetServiceReferences("org.mitk.services.FileReader", filter);
  result.sort();
  std::reverse(result.begin(), result.end());
  return result;
}
