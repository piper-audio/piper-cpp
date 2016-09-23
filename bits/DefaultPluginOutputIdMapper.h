/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Vampipe

    Centre for Digital Music, Queen Mary, University of London.
    Copyright 2006-2016 Chris Cannam and QMUL.
  
    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
    ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    Except as contained in this notice, the names of the Centre for
    Digital Music; Queen Mary, University of London; and Chris Cannam
    shall not be used in advertising or otherwise to promote the sale,
    use or other dealings in this Software without prior written
    authorization.
*/

#ifndef VAMPIPE_DEFAULT_PLUGIN_ID_MAPPER_H
#define VAMPIPE_DEFAULT_PLUGIN_ID_MAPPER_H

#include <vamp-hostsdk/Plugin.h>

namespace vampipe {

class DefaultPluginOutputIdMapper : public PluginOutputIdMapper
{
public:
    DefaultPluginOutputIdMapper(Vamp::Plugin *p) {
	Vamp::Plugin::OutputList outputs = p->getOutputDescriptors();
	for (const auto &d: outputs) {
	    m_ids.push_back(d.identifier);
	}
    }

    virtual int idToIndex(std::string outputId) const noexcept {
	int n = int(m_ids.size());
	for (int i = 0; i < n; ++i) {
	    if (outputId == m_ids[i]) {
		return i;
	    }
	}
	return -1;
    }

    virtual std::string indexToId(int index) const noexcept {
        if (index < 0 || size_t(index) >= m_ids.size()) return "";
	return m_ids[index];
    }

private:
    std::vector<std::string> m_ids;
};

}

#endif
