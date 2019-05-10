/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014-2017,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cs-entry-impl.hpp"

namespace nfd {
namespace cs {

//EntryImpl::EntryImpl(const Name& name)
//  : m_queryName(name)
//{
 // BOOST_ASSERT(this->isQuery());
//}
EntryImpl::EntryImpl(const std::string hashCode)
  : query_hashCode(hashCode)
{
  BOOST_ASSERT(this->isQuery());
}

//EntryImpl::EntryImpl(const Name& name)
 // : query_name(name)
//{
 // BOOST_ASSERT(this->isQuery());
//}
EntryImpl::EntryImpl(shared_ptr<const Data> data, bool isUnsolicited,std::string hash)//emplace 
{
  this->setData(data, isUnsolicited,hash);
  BOOST_ASSERT(!this->isQuery());
}

bool
EntryImpl::isQuery() const
{
  return !this->hasData();
}

void
EntryImpl::unsetUnsolicited()
{
  BOOST_ASSERT(!this->isQuery());
  this->setData(this->getData(), false,"");
}
//
bool
EntryImpl::operator<(const EntryImpl& other) const
{
 if (this->isQuery()) {
    if (other.isQuery()) {
      return query_hashCode.compare(other.query_hashCode) < 0;
    }
    else {//other is not a name
      return query_hashCode.compare(other.getHashCode()) < 0;
    }
  }
  else {//this is not name
    if (other.isQuery()) {
      return other.query_hashCode.compare(this->getHashCode()) > 0;
    }
    else {
      return this->getHashCode().compare(other.getHashCode()) < 0;
    }
  }
}

} // namespace cs
} // namespace nfd
