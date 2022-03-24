/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 IITP RAS
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Based on
 *      NS-2 AODV model developed by the CMU/MONARCH group and optimized and
 *      tuned by Samir Das and Mahesh Marina, University of Cincinnati;
 *
 *      AODV-UU implementation by Erik Nordström of Uppsala University
 *      http://core.it.uu.se/core/index.php/AODV-UU
 *
 * Authors: Elena Buchatskaia <borovkovaes@iitp.ru>
 *          Pavel Boyko <boyko@iitp.ru>
 */
#ifndef MADAODVPACKET_H
#define MADAODVPACKET_H

#include <iostream>
#include "ns3/header.h"
#include "ns3/enum.h"
//#include "ns3/ipv4-address.h"
#include "ns3/mac48-address.h"
#include "ns3/ipv6-address.h"
#include <map>
#include "ns3/nstime.h"

/*
Flags:
No Delete: RERR, 0
--: 1
Gateway Query: RREQ, 2
Unknown Seqno: RREQ, 3
Destination Only: RREQ, 4
Gratuitous RREP: RREQ, 5
Ack Required: RREP, 6
Access Point: RREP, 7

namespace ns3 {
namespace madaodv {
*/

namespace ns3 {
namespace madaodv {

/**
* \ingroup madaodv
* \brief MessageType enumeration
*/
enum MessageType
{
  MADAODVTYPE_RREQ  = 1,   //!< MADAODVTYPE_RREQ
  MADAODVTYPE_RREP  = 2,   //!< MADAODVTYPE_RREP
  MADAODVTYPE_RERR  = 3,   //!< MADAODVTYPE_RERR
  MADAODVTYPE_RREP_ACK = 4 //!< MADAODVTYPE_RREP_ACK
};

/**
* \ingroup madaodv
* \brief MADAODV types
*/
class TypeHeader : public Header
{
public:
  /**
   * constructor
   * \param t the MADAODV RREQ type
   */
  TypeHeader (MessageType t = MADAODVTYPE_RREQ);

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId ();
  TypeId GetInstanceTypeId () const;
  uint32_t GetSerializedSize () const;
  void Serialize (Buffer::Iterator start) const;
  uint32_t Deserialize (Buffer::Iterator start);
  void Print (std::ostream &os) const;

  /**
   * \returns the type
   */
  MessageType Get () const
  {
    return m_type;
  }
  /**
   * Check that type if valid
   * \returns true if the type is valid
   */
  bool IsValid () const
  {
    return m_valid;
  }
  /**
   * \brief Comparison operator
   * \param o header to compare
   * \return true if the headers are equal
   */
  bool operator== (TypeHeader const & o) const;
private:
  MessageType m_type; ///< type of the message
  bool m_valid; ///< Indicates if the message is valid
};

/**
  * \brief Stream output operator
  * \param os output stream
  * \return updated stream
  */
std::ostream & operator<< (std::ostream & os, TypeHeader const & h);

/**
* \ingroup madaodv
* \brief   Route Request (RREQ) Message Format
  \verbatim
   0                 1             2               3            
    0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     Type      |J|R|G|D|U|Q|     Reserved      |   Hop Count   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                            RREQ ID                            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   +                                                               +
   |                                                               |
   +                   Originator IPv6 Address (16)                +          
   |                                                               |
   +                                                               +
   |                                                               |
   +-------------------------------+-------------------------------+
   |                                                               |
   +                                                               +
   |                                                               |
   +                   Destination IPv6 Address (16)               +          
   |                                                               |
   +                                                               +
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                  Destination Sequence Number (4)              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                  Originator Sequence Number (4)               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 
  \endverbatim

*/
class RreqHeader : public Header
{
public:
  /**
   * constructor
   *
   * \param flags the message flags (0)
   * \param reserved the reserved bits (0)
   * \param hopCount the hop count
   * \param requestID the request ID
   * \param dst the destination ipv6 address
   * \param dstSeqNo the destination sequence number
   * \param origin the origin ipv6 address
   * \param originSeqNo the origin sequence number
   */
   RreqHeader (uint8_t flags = 0, uint8_t reserved = 0, uint8_t hopCount = 0,
              uint32_t requestID = 0, Ipv6Address dst = Ipv6Address (),
              uint32_t dstSeqNo = 0, Ipv6Address origin = Ipv6Address (),
              uint32_t originSeqNo = 0);

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId ();
  TypeId GetInstanceTypeId () const;
  uint32_t GetSerializedSize () const;
  void Serialize (Buffer::Iterator start) const;
  uint32_t Deserialize (Buffer::Iterator start);
  void Print (std::ostream &os) const;

  // Fields
  /**
   * \brief Set the hop count
   * \param count the hop count
   */
  void SetHopCount (uint8_t count)
  {
    m_hopCount = count;
  }
  /**
   * \brief Get the hop count
   * \return the hop count
   */
  uint8_t GetHopCount () const
  {
    return m_hopCount;
  }
  /**
   * \brief Set the request ID
   * \param id the request ID
   */
  void SetId (uint32_t id)
  {
    m_requestID = id;
  }
  /**
   * \brief Get the request ID
   * \return the request ID
   */
  uint32_t GetId () const
  {
    return m_requestID;
  }
  /**
   * \brief Set the destination address
   * \param a the destination address
   */
  void SetDst (Ipv6Address a)
  {
    m_dst = a;
  }
  /**
   * \brief Get the destination address
   * \return the destination address
   */
  Ipv6Address GetDst () const
  {
    return m_dst;
  }
  /**
   * \brief Set the destination sequence number
   * \param s the destination sequence number
   */
  void SetDstSeqno (uint32_t s)
  {
    m_dstSeqNo = s;
  }
  /**
   * \brief Get the destination sequence number
   * \return the destination sequence number
   */
  uint32_t GetDstSeqno () const
  {
    return m_dstSeqNo;
  }
  /**
   * \brief Set the origin address
   * \param a the origin address
   */
  void SetOrigin (Ipv6Address a)
  {
    m_origin = a;
  }
  /**
   * \brief Get the origin address
   * \return the origin address
   */
  Ipv6Address GetOrigin () const
  {
    return m_origin;
  }
  /**
   * \brief Set the origin sequence number
   * \param s the origin sequence number
   */
  void SetOriginSeqno (uint32_t s)
  {
    m_originSeqNo = s;
  }
  
  /**
   * \brief Get the origin sequence number
   * \return the origin sequence number
   */
  uint32_t GetOriginSeqno () const
  {
    return m_originSeqNo;
  }

  // Flags
  /**
   * \brief Set the gratuitous RREP flag
   * \param f the gratuitous RREP flag
   */
  void SetGratuitousRrep (bool f);
  /**
   * \brief Get the gratuitous RREP flag
   * \return the gratuitous RREP flag
   */
  bool GetGratuitousRrep () const;
  /**
   * \brief Set the Destination only flag
   * \param f the Destination only flag
   */
  void SetDestinationOnly (bool f);
  /**
   * \brief Get the Destination only flag
   * \return the Destination only flag
   */
  bool GetDestinationOnly () const;
  /**
   * \brief Set the unknown sequence number flag
   * \param f the unknown sequence number flag
   */
  void SetUnknownSeqno (bool f);
  /**
   * \brief Get the unknown sequence number flag
   * \return the unknown sequence number flag
   */
  bool GetUnknownSeqno () const;
  /**
   * \brief Set the gateway query flag
   * \param f the gateway query flag
   */
  void SetAccessPointQuery (bool f);
  /**
   * \brief Get the gateway query flag
   * \return the gateway query flag
   */
  bool GetAccessPointQuery () const;


  /**
   * \brief Comparison operator
   * \param o RREQ header to compare
   * \return true if the RREQ headers are equal
   */
  bool operator== (RreqHeader const & o) const;
private:
  uint8_t        m_flags;          ///< |J|R|G|D|U|Q| bit flags, see RFC, Q  for query gateway flag
  uint8_t        m_reserved;       ///< Not used (must be 0)
  uint8_t        m_hopCount;       ///< Hop Count
  uint32_t       m_requestID;      ///< RREQ ID
  Ipv6Address    m_dst;            ///< Destination Ipv6 Address
  uint32_t       m_dstSeqNo;       ///< Destination Sequence Number
  Ipv6Address    m_origin;         ///< Originator Ipv6 Address
  uint32_t       m_originSeqNo;    ///< Source Sequence Number
};

/**
  * \brief Stream output operator
  * \param os output stream
  * \return updated stream
  */
std::ostream & operator<< (std::ostream & os, RreqHeader const &);

/**
* \ingroup madaodv
* \brief Route Reply (RREP) Message Format
  \verbatim
  \verbatim
   0                 1             2               3            
    0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     Type      |P|A|         Reserved          |   Hop Count   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   +                                                               +
   |                                                               |
   +                   Originator IPv6 Address (16)                +          
   |                                                               |
   +                                                               +
   |                                                               |
   +-------------------------------+-------------------------------+
   |                                                               |
   +                                                               +
   |                                                               |
   +                   Destination IPv6 Address (16)               +          
   |                                                               |
   +                                                               +
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                  Destination Sequence Number (4)              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                            Lifetime (4)                       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 
  \endverbatim

//SET FLAGS?
*/
class RrepHeader : public Header
{
public:
  /**
   * constructor
   * \param flags P for Access Point, A for acknowledgement required
   * \param reserved reserved bits (0)
   * \param hopCount the hop count (0)
   * \param dst the destination Ipv6 address
   * \param dstSeqNo the destination sequence number
   * \param origin the origin Ipv6 address
   * \param lifetime the lifetime
   */
  RrepHeader (uint8_t reserved = 0, uint8_t hopCount = 0, Ipv6Address dst =
                Ipv6Address (), uint32_t dstSeqNo = 0, Ipv6Address origin =
                Ipv6Address (), Time lifetime = MilliSeconds (0));
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId ();
  TypeId GetInstanceTypeId () const;
  uint32_t GetSerializedSize () const;
  void Serialize (Buffer::Iterator start) const;
  uint32_t Deserialize (Buffer::Iterator start);
  void Print (std::ostream &os) const;

  // Fields
  /**
   * \brief Set the hop count
   * \param count the hop count
   */
  void SetHopCount (uint8_t count)
  {
    m_hopCount = count;
  }
  /**
   * \brief Get the hop count
   * \return the hop count
   */
  uint8_t GetHopCount () const
  {
    return m_hopCount;
  }
  /**
   * \brief Set the destination address
   * \param a the destination address
   */
  void SetDst (Ipv6Address a)
  {
    m_dst = a;
  }
  /**
   * \brief Get the destination address
   * \return the destination address
   */
  Ipv6Address GetDst () const
  {
    return m_dst;
  }
  /**
   * \brief Set the destination sequence number
   * \param s the destination sequence number
   */
  void SetDstSeqno (uint32_t s)
  {
    m_dstSeqNo = s;
  }
  /**
   * \brief Get the destination sequence number
   * \return the destination sequence number
   */
  uint32_t GetDstSeqno () const
  {
    return m_dstSeqNo;
  }
  /**
   * \brief Set the origin address
   * \param a the origin address
   */
  void SetOrigin (Ipv6Address a)
  {
    m_origin = a;
  }
  /**
   * \brief Get the origin address
   * \return the origin address
   */
  Ipv6Address GetOrigin () const
  {
    return m_origin;
  }
  /**
   * \brief Set the lifetime
   * \param t the lifetime
   */
  void SetLifeTime (Time t);
  /**
   * \brief Get the lifetime
   * \return the lifetime
   */
  Time GetLifeTime () const;

  // Flags
  /**
   * \brief Set the ack required flag
   * \param f the ack required flag
   */
  void SetAckRequired (bool f);
  /**
   * \brief get the ack required flag
   * \return the ack required flag
   */
  bool GetAckRequired () const;
  /**
   * \brief Set the access point flag
   * \param f the access point flag
   */
  void SetAccessPoint (bool f);
  /**
   * \brief get the access point flag
   * \return the access point flag
   */
  bool GetAccessPoint () const;

  /**
   * Configure RREP to be a Hello message
   *
   * \param src the source IP address
   * \param srcSeqNo the source sequence number
   * \param lifetime the lifetime of the message
   */
  void SetHello (Ipv6Address src, uint32_t srcSeqNo, Time lifetime);

  /**
   * \brief Comparison operator
   * \param o RREP header to compare
   * \return true if the RREP headers are equal
   */
  bool operator== (RrepHeader const & o) const;
private:
  uint8_t       m_flags;            ///< A, P - acknowledgment required, access point flags
  uint8_t       m_reserved;         ///< Reserved 
  uint8_t             m_hopCount;   ///< Hop Count
  Ipv6Address   m_dst;             ///< Destination Ipv6 Address
  uint32_t      m_dstSeqNo;         ///< Destination Sequence Number
  Ipv6Address     m_origin;        ///< Source Ipv6 Address
  uint32_t      m_lifeTime;         ///< Lifetime (in milliseconds)
};

/**
  * \brief Stream output operator
  * \param os output stream
  * \return updated stream
  */
std::ostream & operator<< (std::ostream & os, RrepHeader const &);

/**
* \ingroup madaodv
* \brief Route Reply Acknowledgment (RREP-ACK) Message Format
  \verbatim
  0                   1
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |     Type      |   Reserved    |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  \endverbatim
*/
class RrepAckHeader : public Header
{
public:
  /// constructor
  RrepAckHeader ();

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId ();
  TypeId GetInstanceTypeId () const;
  uint32_t GetSerializedSize () const;
  void Serialize (Buffer::Iterator start) const;
  uint32_t Deserialize (Buffer::Iterator start);
  void Print (std::ostream &os) const;

  /**
   * \brief Comparison operator
   * \param o RREP header to compare
   * \return true if the RREQ headers are equal
   */
  bool operator== (RrepAckHeader const & o) const;
private:
  uint8_t       m_reserved; ///< Not used (must be 0)
};

/**
  * \brief Stream output operator
  * \param os output stream
  * \return updated stream
  */
std::ostream & operator<< (std::ostream & os, RrepAckHeader const &);


/**
* \ingroup madaodv
* \brief Route Error (RERR) Message Format
  \verbatim
   0                 1             2               3            
    0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     Type      |N|           Reserved          |   Hop Count   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   +                                                               +
   |                                                               |
   +                  Unreachable IPv6 Address (16)                +          
   |                                                               |
   +                                                               +
   |                                                               |
   +-------------------------------+-------------------------------+ 
   |                   Unreachable Dest Seqno (4)                  |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
   |                                                               |
   +                                                               +
   |                                                               |
   +             Additional Unreachable IPv6 Address (16)          +          
   |                                                               |
   +                                                               +
   |                                                               |
   +-------------------------------+-------------------------------+ 
   |             Additional Unreachable Dest Seqno (4)             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 


  \endverbatim

*/
class RerrHeader : public Header
{
public:
  /// constructor
  RerrHeader ();

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId ();
  TypeId GetInstanceTypeId () const;
  uint32_t GetSerializedSize () const;
  void Serialize (Buffer::Iterator i) const;
  uint32_t Deserialize (Buffer::Iterator start);
  void Print (std::ostream &os) const;

  // No delete flag
  /**
   * \brief Set the no delete flag
   * \param f the no delete flag
   */
  void SetNoDelete (bool f);
  /**
   * \brief Get the no delete flag
   * \return the no delete flag
   */
  bool GetNoDelete () const;

  /**
   * \brief Add unreachable node address and its sequence number in RERR header
   * \param dst unreachable MAC address
   * \param seqNo unreachable sequence number
   * \return false if we already added maximum possible number of unreachable destinations
   */
  bool AddUnDestination (Ipv6Address dst, uint32_t seqNo);
  /**
   * \brief Delete pair (address + sequence number) from RERR header, if the number of unreachable destinations > 0
   * \param un unreachable pair (address + sequence number)
   * \return true on success
   */
  bool RemoveUnDestination (std::pair<Ipv6Address, uint32_t> & un);
  /// Clear header
  void Clear ();
  /**
   * \returns number of unreachable destinations in RERR message
   */
  uint8_t GetDestCount () const
  {
    return (uint8_t)m_unreachableDstSeqNo.size ();
  }

  /**
   * \brief Comparison operator
   * \param o RERR header to compare
   * \return true if the RERR headers are equal
   */
  bool operator== (RerrHeader const & o) const;
private:
  uint8_t m_flag;            ///< No delete flag
  uint8_t m_reserved;        ///< Not used (must be 0)

  /// List of Unreachable destination: IP addresses and sequence numbers
  std::map<Ipv6Address, uint32_t> m_unreachableDstSeqNo;
};

/**
  * \brief Stream output operator
  * \param os output stream
  * \return updated stream
  */
std::ostream & operator<< (std::ostream & os, RerrHeader const &);

}  // namespace madaodv
}  // namespace ns3

#endif /* MADAODVPACKET_H */
