/* ------------------------------------------------------------------
 * Copyright (C) 1998-2009 PacketVideo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */
/*
    This PVA_FF_MediaHeaderAtom Class contains all the objects that declare information
    about the media data within the stream.
*/


#define IMPLEMENT_MediaHeaderAtom

#include "mediaheaderatom.h"
#include "atomutils.h"
#include "a_atomdefs.h"
#include "oscl_int64_utils.h"


// Constructor
PVA_FF_MediaHeaderAtom::PVA_FF_MediaHeaderAtom(uint8 version)
        : PVA_FF_FullAtom(MEDIA_HEADER_ATOM, version, 0)
{
    init(); // Initialize all member variables
    recomputeSize();
}

// Destructor
PVA_FF_MediaHeaderAtom::~PVA_FF_MediaHeaderAtom()
{
    // Empty
}


// Initialize all members
void
PVA_FF_MediaHeaderAtom::init()
{
    // use a temporary variable to set the current time since the API setTime takes uint32 parameters
    uint32 timeSet;
    PVA_FF_AtomUtils::setTime(timeSet); // Setting creating time (since 1/1/1970) - NEED FIX to 1/1/1904
    _creationTime = timeSet;
    PVA_FF_AtomUtils::setTime(timeSet); // Setting modification time
    _modificationTime = timeSet;
    _timeScale = DEFAULT_PRESENTATION_TIMESCALE;
    _duration = 0;
    _language = 0; // Until find better default value
    _reserved = 0;

    /*
     * Variables added to ensure that the total track duration includes the duration of the
     * last sample as well, which in our case fp same as the last but one.
     */
    _prevTS = 0;
    _deltaTS = 0;
}

// Inform header that a new sample fp added to allow it to maintain its
// duration parameter - in terms of the media timescale - i.e. a sample count
void
PVA_FF_MediaHeaderAtom::addSample(uint32 ts)
{
    _deltaTS = ts - _prevTS;

    setDuration(ts);

    _prevTS = ts;
}


// in movie fragment mode set the actual duration of
// last sample
void
PVA_FF_MediaHeaderAtom::updateLastTSEntry(uint32 ts)
{

    setDuration(ts);
    _deltaTS = 0;   // made 0 so that duration can not be updated before rendoring
    // in movie fragment mode
}



// Recompute size of atom
void
PVA_FF_MediaHeaderAtom::recomputeSize()
{
    int32 size = getDefaultSize(); // Get size of base class members

    // Fields that vary depending on the version
    if (getVersion() == 0)
    {
        size += 4; //_creationTime
        size += 4; //_modificationTime
        size += 4; //_timeScale
        size += 4; //_duration
    }
    else // getVersion() == 1
    {
        size += 8; //_creationTime
        size += 8; //_modificationTime
        size += 4; //_timeScale
        size += 8; //_duration
    }

    size += 2; // language
    size += 2; // reserved

    _size = size;

    // Update the size of the parent atom
    if (_pparent != NULL)
    {
        _pparent->recomputeSize();
    }
}

// Rendering the PVA_FF_Atom in proper format (bitlengths, etc.) to an ostream
bool
PVA_FF_MediaHeaderAtom::renderToFileStream(MP4_AUTHOR_FF_FILE_IO_WRAP *fp)
{
    int32 rendered = 0; // Keep track of number of bytes rendered

    // Render PVA_FF_Atom type and size
    if (!renderAtomBaseMembers(fp))
    {
        return false;
    }
    rendered += getDefaultSize();

    if (getVersion() == 0)
    {
        if (!PVA_FF_AtomUtils::render32(fp,
                                        Oscl_Int64_Utils::get_uint64_lower32(getCreationTime())))
        {
            return false;
        }

        if (!PVA_FF_AtomUtils::render32(fp,
                                        Oscl_Int64_Utils::get_uint64_lower32(getModificationTime())))

        {
            return false;
        }
    }
    else // getVersion() == 1
    {
        if (!PVA_FF_AtomUtils::render64(fp, getCreationTime()))
        {
            return false;
        }
        if (!PVA_FF_AtomUtils::render64(fp, getModificationTime()))
        {
            return false;
        }
    }

    if (!PVA_FF_AtomUtils::render32(fp, getTimeScale()))
    {
        return false;
    }

    /*
     * To ensure that the total track duration includes the duration of the
     * last sample as well, which in our case fp same as the last but one.
     */
    uint64 totalDuration = getDuration() + _deltaTS;

    if (getVersion() == 0)
    {
        if (!PVA_FF_AtomUtils::render32(fp,
                                        Oscl_Int64_Utils::get_uint64_lower32(totalDuration)))
        {
            return false;
        }
    }
    else // getVersion() == 1
    {
        if (!PVA_FF_AtomUtils::render64(fp, totalDuration))
        {
            return false;
        }
    }

    if (getVersion() == 0)
    {
        rendered += 16;
    }
    else
    {
        rendered += 28;
    }

    if (!PVA_FF_AtomUtils::render16(fp, getLanguage()))
    {
        return false;
    }
    rendered += 2;

    if (!PVA_FF_AtomUtils::render16(fp, _reserved))
    {
        return false;
    }
    rendered += 2;

    return true;
}


