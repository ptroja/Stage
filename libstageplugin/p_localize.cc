/*
 *  Player - One Hell of a Robot Server
 *  Copyright (C) 2004, 2005 Richard Vaughan
 *                      
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
 * Desc: A plugin driver for Player that gives access to Stage devices.
 * Author: Richard Vaughan
 * Date: 10 December 2004
 * CVS: $Id$
 */

// DOCUMENTATION ------------------------------------------------------------

/** @addtogroup player 
@par Localize interface
- PLAYER_LOCALIZE_DATA_HYPOTHS
*/

// CODE ----------------------------------------------------------------------

#include "p_driver.h" 
using namespace Stg;

InterfaceLocalize::InterfaceLocalize( player_devaddr_t addr, 
				StgDriver* driver,
				ConfigFile* cf,
				int section )
  : InterfaceModel( addr, driver, cf, section, "" )
{ 
}

void InterfaceLocalize::Publish( void )
{
  player_localize_data_t loc;
  memset( &loc, 0, sizeof(loc));

  Pose pose = this->mod->GetPose();

  // only 1 hypoth - it's the truth!
  loc.hypoths_count = 1;

  player_localize_hypoth_t truth;

  truth.mean.px = pose.x;
  truth.mean.py = pose.y;
  truth.mean.pa = pose.a;
  for(int i = 0; i < 6; ++i)
    truth.cov[i] = 0.0;
  truth.alpha = 1.0;

  loc.hypoths = &truth;

  // Write localize data
  this->driver->Publish(this->addr,
			PLAYER_MSGTYPE_DATA,
			PLAYER_LOCALIZE_DATA_HYPOTHS,
			(void*) &loc);
}


int InterfaceLocalize::ProcessMessage(QueuePointer & resp_queue,
				      player_msghdr_t* hdr,
				      void* data)
{
  // Is it a new motor command?
  if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_REQ,
                           PLAYER_LOCALIZE_REQ_SET_POSE,
                           this->addr))
  {
    // convert from Player to Stage format
    player_localize_set_pose_t* cmd = (player_localize_set_pose_t*)data;

    Pose pose;
    pose.x = cmd->mean.px;
    pose.y = cmd->mean.py;
    pose.a = cmd->mean.pa;
    this->mod->SetPose(pose);

    this->driver->Publish( this->addr, resp_queue,
                           PLAYER_MSGTYPE_RESP_ACK,
                           PLAYER_LOCALIZE_REQ_SET_POSE);

    return 0;
  }

  // Is it a new motor command?
  if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_REQ,
                           PLAYER_LOCALIZE_REQ_GET_PARTICLES,
                           this->addr))
  {
    player_localize_get_particles_t loc;

    Pose pose = this->mod->GetPose();

    loc.mean.px = pose.x;
    loc.mean.py = pose.y;
    loc.mean.pa = pose.a;
    loc.particles_count = 0;
    loc.variance = 0.0;

    // Write localize data
    this->driver->Publish(this->addr, resp_queue,
  			PLAYER_MSGTYPE_RESP_ACK,
  			PLAYER_LOCALIZE_REQ_GET_PARTICLES,
  			(void*) &loc);

    return 0;
  }

  // Don't know how to handle this message.
  PRINT_WARN2( "stage localize doesn't support message %d:%d.",
	       hdr->type, hdr->subtype);
  return(-1);
}
