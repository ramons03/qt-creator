/***************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2008 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact:  Qt Software Information (qt-info@nokia.com)
**
**
** Non-Open Source Usage
**
** Licensees may use this file in accordance with the Qt Beta Version
** License Agreement, Agreement version 2.2 provided with the Software or,
** alternatively, in accordance with the terms contained in a written
** agreement between you and Nokia.
**
** GNU General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU General
** Public License versions 2.0 or 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the packaging
** of this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
**
** http://www.fsf.org/licensing/licenses/info/GPLv2.html and
** http://www.gnu.org/copyleft/gpl.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt GPL Exception
** version 1.3, included in the file GPL_EXCEPTION.txt in this package.
**
***************************************************************************/
// Copyright (c) 2008 Roberto Raggi <roberto.raggi@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "MemoryPool.h"
#include <cstdlib>
#include <cstring>
#include <cassert>

CPLUSPLUS_BEGIN_NAMESPACE

MemoryPool::MemoryPool()
    : _initializeAllocatedMemory(true),
      _blocks(0),
      _allocatedBlocks(0),
      _blockCount(-1),
      ptr(0),
      end(0)
{ }

MemoryPool::~MemoryPool()
{
    if (_blockCount != -1) {
        for (int i = 0; i < _blockCount + 1; ++i) {
            free(_blocks[i]);
        }
    }

    if (_blocks)
        free(_blocks);
}

bool MemoryPool::initializeAllocatedMemory() const
{ return _initializeAllocatedMemory; }

void MemoryPool::setInitializeAllocatedMemory(bool initializeAllocatedMemory)
{ _initializeAllocatedMemory = initializeAllocatedMemory; }

void *MemoryPool::allocate_helper(size_t size)
{
    assert(size < BLOCK_SIZE);

    if (++_blockCount == _allocatedBlocks) {
        if (! _allocatedBlocks)
            _allocatedBlocks = 8;
        else
            _allocatedBlocks *= 2;

        _blocks = (char **) realloc(_blocks, sizeof(char *) * _allocatedBlocks);
    }

    char *&block = _blocks[_blockCount];

    if (_initializeAllocatedMemory)
        block = (char *) calloc(1, BLOCK_SIZE);
    else
        block = (char *) malloc(BLOCK_SIZE);

    ptr = block;
    end = ptr + BLOCK_SIZE;

    void *addr = ptr;
    ptr += size;
    return addr;
}

Managed::Managed()
{ }

Managed::~Managed()
{ }

void *Managed::operator new(size_t size, MemoryPool *pool)
{ return pool->allocate(size); }

void Managed::operator delete(void *)
{ }

void Managed::operator delete(void *, MemoryPool *)
{ }

CPLUSPLUS_END_NAMESPACE
