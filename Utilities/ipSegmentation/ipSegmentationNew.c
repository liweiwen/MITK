/*=========================================================================
 
Program:   Medical Imaging & Interaction Toolkit
Module:    $RCSfile: mitkPropertyManager.cpp,v $
Language:  C++
Date:      $Date$
Version:   $Revision$
 
Copyright (c) German Cancer Research Center, Division of Medical and
Biological Informatics. All rights reserved.
See MITKCopyright.txt or http://www.mitk.org/copyright.html for details.
 
This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notices for more information.
 
=========================================================================*/

#include "ipSegmentationP.h"

ipPicDescriptor*
ipMITKSegmentationNew (ipPicDescriptor* image)
{
    ipPicDescriptor* s = NULL;

    if (image) {
        s = ipPicNew ();
        s->type = ipMITKSegmentationTYPE_ID;
        s->bpe = ipMITKSegmentationBPE;
        s->dim = 2;
        s->n[0] = image->n[0];
        s->n[1] = image->n[1];
        s->data = malloc (_ipPicSize (s));
    }
    return s;
}
