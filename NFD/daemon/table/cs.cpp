/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2019,  Regents of the University of California,
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

#include "cs.hpp"
#include "core/algorithm.hpp"
#include "core/logger.hpp"

#include <ndn-cxx/lp/tags.hpp>
#include <ndn-cxx/util/concepts.hpp>
#include <ndn-cxx/data.hpp>
namespace nfd {
namespace cs {

NDN_CXX_ASSERT_FORWARD_ITERATOR(Cs::const_iterator);

NFD_LOG_INIT(ContentStore);

static unique_ptr<Policy>
makeDefaultPolicy()
{
  return Policy::create("lru");
}

Cs::Cs(size_t nMaxPackets)
{
  setPolicyImpl(makeDefaultPolicy());
  m_policy->setLimit(nMaxPackets);
}

int
Cs::insert(const Data& data, bool isUnsolicited,std::string hashCode)//1 for insert a new entry, 0 for a old entry, -1 for a error
{
	  printf("------------------------------insert in CS-------------------------------") ;
  if (!m_shouldAdmit || m_policy->getLimit() == 0) {
    return -1;
  }
  NFD_LOG_DEBUG("insert " << data.getName());

  // recognize CachePolicy
  shared_ptr<lp::CachePolicyTag> tag = data.getTag<lp::CachePolicyTag>();
  if (tag != nullptr) {
    lp::CachePolicyType policy = tag->get().getPolicy();
    if (policy == lp::CachePolicyType::NO_CACHE) {
      return -1;
    }
  }
//  iterator iter;
//  iter =  m_table.begin();
// here to compare hashcode
 // std::string hashCode = data.getHash();//here to compute the hash, the only place to compute
 // while (iter!=m_table.end())
 // {
  //  if(iter->getHashCode().compare(hashCode)){
  //      m_policy->afterRefresh(iter);
         
  //      return -1;
 //   }
 //   iter++;
 // }
  iterator iter = m_table.lower_bound(hashCode);
  if(iter->getHashCode() == hashCode){
     m_policy->afterRefresh(iter);
 printf("------------------------------insert in CS:HIT a same hash-------------------------------") ;
     return -1;
  }

  iterator it;
  bool isNewEntry = false;        
  std::tie(it, isNewEntry) = m_table.emplace(data.shared_from_this(), isUnsolicited,hashCode);//perform EntryImpl
//it is referred to isNewEntry, which is a bool ,is emplace is performed, it returns a point to the location of the entry 
// emplace = add(a),std::set have not duplicated, so if repudicate, return 0 insert false
  EntryImpl& entry = const_cast<EntryImpl&>(*it);
  entry.updateStaleTime();
  if (!isNewEntry) { // existing entry
    // XXX This doesn't forbid unsolicited Data from refreshing a solicited entry, although unsolicited Data is not inserted     
    if (entry.isUnsolicited() && !isUnsolicited) {
      entry.unsetUnsolicited();//set isunsolisited = false
    }
    m_policy->afterRefresh(it);
    return 0;// if old
  }
  else {
    m_policy->afterInsert(it);
    return 1;//if new
  }
}


void
Cs::erase(const Name& prefix, size_t limit, const AfterEraseCallback& cb)
{
  BOOST_ASSERT(static_cast<bool>(cb));
  iterator first = m_table.begin();
  iterator last = m_table.end();
  iterator match = last;
  while(first != last){
     if((first->getNameFromEntry()) == prefix){
       match = first;
       break;
     }
     first++;
   }
  if(match != last){//have some problems
     m_policy->beforeErase(match);
     match = m_table.erase(match);
     if (cb) {
         cb(1);
     }
   }
}


void
Cs::find(const Interest& interest,
         const HitCallback& hitCallback,
         const MissCallback& missCallback) const
{
  BOOST_ASSERT(static_cast<bool>(hitCallback));
  BOOST_ASSERT(static_cast<bool>(missCallback));
  printf("------------------------------CS::find-------------------------------") ;
  if (!m_shouldServe || m_policy->getLimit() == 0) {
    missCallback(interest);
    return;
  }
  iterator last = m_table.upper_bound(interest.getHashCode());
  iterator match = m_table.lower_bound(interest.getHashCode());
if(last != match)
{
    NFD_LOG_DEBUG("  no-match");
    missCallback(interest);
    return;
 }
  NFD_LOG_DEBUG("  matching " << match->getHashCode());
  m_policy->beforeUse(match);
  hitCallback(interest, match->getData());
}
void
Cs::dump()
{
  NFD_LOG_DEBUG("dump table");
  for (const EntryImpl& entry : m_table) {
    NFD_LOG_TRACE(entry.getFullName());
  }
}

void
Cs::setPolicy(unique_ptr<Policy> policy)
{
  BOOST_ASSERT(policy != nullptr);
  BOOST_ASSERT(m_policy != nullptr);
  size_t limit = m_policy->getLimit();
  this->setPolicyImpl(std::move(policy));
  m_policy->setLimit(limit);
}

void
Cs::setPolicyImpl(unique_ptr<Policy> policy)
{
  NFD_LOG_DEBUG("set-policy " << policy->getName());
  m_policy = std::move(policy);
  m_beforeEvictConnection = m_policy->beforeEvict.connect([this] (iterator it) {
      m_table.erase(it);
    });

  m_policy->setCs(this);
  BOOST_ASSERT(m_policy->getCs() == this);
}

void
Cs::enableAdmit(bool shouldAdmit)
{
  if (m_shouldAdmit == shouldAdmit) {
    return;
  }
  m_shouldAdmit = shouldAdmit;
  NFD_LOG_INFO((shouldAdmit ? "Enabling" : "Disabling") << " Data admittance");
}

void
Cs::enableServe(bool shouldServe)
{
  if (m_shouldServe == shouldServe) {
    return;
  }
  m_shouldServe = shouldServe;
  NFD_LOG_INFO((shouldServe ? "Enabling" : "Disabling") << " Data serving");
}

} // namespace cs
} // namespace nfd
