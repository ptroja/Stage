// $Id: pioneermobiledevice.cc,v 1.5 2000-12-02 03:25:58 vaughan Exp $	
#include <math.h>

#include "world.h"
#include "robot.h"
#include "pioneermobiledevice.h"

const double TWOPI = 6.283185307;

// constructor

CPioneerMobileDevice::CPioneerMobileDevice( CRobot* rr, 
					    double wwidth, double llength,
					    void *buffer, 
					    size_t data_len, 
					    size_t command_len, 
					    size_t config_len)
        : CPlayerDevice(rr, buffer, data_len, command_len, config_len)
{
  m_robot = rr;
  m_world = m_robot->world;

  ASSERT(m_robot != NULL);
  ASSERT(m_world != NULL);
  
  m_update_interval = 0.01; // update me very fast indeed

  width = wwidth;
  length = llength;

  halfWidth = width / 2.0;   // the halves are used often in calculations
  halfLength = length / 2.0;

  xodom = yodom = aodom = 0;

  speed = 0.0;
  turnRate = 0.0;

  stall = 0;

  memset( &rect, 0, sizeof( rect ) );
  memset( &oldRect, 0, sizeof( rect ) );

  CalculateRect();
}

///////////////////////////////////////////////////////////////////////////
// Update the position of the robot base
//
bool CPioneerMobileDevice::Update()
{
  //TRACE0("updating CPioneerMobileDevice");
 
  // generic device call  
  if( GetCommand( commands, P2OS_COMMAND_BUFFER_SIZE ) 
      == P2OS_COMMAND_BUFFER_SIZE )
    {
      ParseCommandBuffer();    // find out what to do    
    }
  
  MapUnDraw(); // erase myself
   
  Move();      // do things
    
  MapDraw();   // draw myself 

  ComposeData();     // report the new state of things
  PutData( data, P2OS_DATA_BUFFER_SIZE  );     // generic device call

  return true;
}


int CPioneerMobileDevice::Move()
{
  Nimage* img = m_world->img;

  StoreRect(); // save my current rectangle in cased I move

  int moved = false;

  if( ( m_world->win && m_world->win->dragging != m_robot )  
      &&  (speed != 0.0 || turnRate != 0.0) )
    {
      // record speeds now - they can be altered in another thread
      float nowSpeed = speed;
      float nowTurn = turnRate;
      float nowTimeStep = m_world->timeStep;

      // find the new position for the robot
      float tx = m_robot->x 
	+ nowSpeed * m_world->ppm * cos( m_robot->a ) * nowTimeStep;
      
      float ty = m_robot->y 
	+ nowSpeed * m_world->ppm * sin( m_robot->a ) * nowTimeStep;
      
      float ta = m_robot->a + (nowTurn * nowTimeStep);
      
      ta = fmod( ta + TWOPI, TWOPI );  // normalize angle
      
      // calculate the rectangle for the robot's tentative new position
      CalculateRect( tx, ty, ta );

      //cout << "txya " << tx << ' ' << ty << ' ' << ta << endl;

      // trace the robot's outline to see if it will hit anything
      char hit = 0;
      if( hit = img->rect_detect( rect, m_robot->color ) > 0 )
	// hit! so don't do the move - just redraw the robot where it was
	{
	  //cout << "HIT! " << endl;
	  // restore from the saved rect
	  memcpy( &rect, &oldRect, sizeof( struct Rect ) );
	  
	  stall = 1; // motors stalled due to collision
	}
      else // do the move
	{
	  moved = true;
	  
	  stall = 0; // motors not stalled
	  
	  m_robot->x = tx; // move to the new position
	  m_robot->y = ty;
	  m_robot->a = ta;
	  
	  //update the robot's odometry estimate
	  xodom +=  nowSpeed * m_world->ppm * cos( aodom ) * nowTimeStep;
	  yodom +=  nowSpeed * m_world->ppm * sin( aodom ) * nowTimeStep;
	  
	  if( m_world->maxAngularError != 0.0 ) //then introduce some error
	    {
	      float error = 1.0 + ( drand48()* m_world->maxAngularError );
	      aodom += nowTurn * nowTimeStep * error;
	    }
	  else
	    aodom += nowTurn * nowTimeStep;
	  
	  aodom = fmod( aodom + TWOPI, TWOPI );  // normalize angle
	 	  
	  // update the `old' stuff
	  memcpy( &oldRect, &rect, sizeof( struct Rect ) );
	  oldCenterx = centerx;
	  oldCentery = centery;
	}
    }
  
  m_robot->oldx = m_robot->x;
  m_robot->oldy = m_robot->y;
  m_robot->olda = m_robot->a;
 
  return moved;
}

void CPioneerMobileDevice::ComposeData()
{
  // this will put the data into P2OS packet format to be shipped
  // to Player

  unsigned char* deviceData = data;
  
  double rtod = 180.0/M_PI;
 
  // position device
  *((int*)(deviceData + 0)) =
    htonl((int)((m_world->timeNow - m_world->timeBegan) * 1000.0));
  
  *((int*)(deviceData + 4)) = 
    htonl((int)((m_robot->x - m_robot->xorigin)/ m_world->ppm * 1000.0));
  
  *((int*)(deviceData + 8)) = 
    htonl((int)((m_robot->y - m_robot->yorigin)/ m_world->ppm * 1000.0));

  // normalized odometry heading
  float odoHeading = fmod( m_robot->aorigin - m_robot->a + TWOPI, TWOPI ); 
  *((unsigned short*)(deviceData + 12)) =
    htons((unsigned short)(odoHeading * rtod ));

  // speeds
  *((unsigned short*)(deviceData + 14))
    = htons((unsigned short)speed);

  *((unsigned short*)(deviceData + 16))
    = htons((unsigned short)(turnRate * rtod ));

  // normalized compass heading
  float comHeading = fmod( m_robot->a + M_PI/2.0 + TWOPI, TWOPI ); 
  
  *((unsigned short*)(deviceData + 18))
    = htons((unsigned short)( comHeading * rtod ));

  // stall - currently shows if the robot is being dragged by the user
  *(unsigned char*)(deviceData + 20) = stall;
}

bool CPioneerMobileDevice::MapUnDraw()
{
  m_world->img->draw_rect( oldRect, 0 );
  return 1;
}


bool CPioneerMobileDevice::MapDraw()
{
  // calculate my new rectangle
  CalculateRect( m_robot->x, m_robot->y, m_robot->a );

  m_world->img->draw_rect( rect, m_robot->color );

  StoreRect();
  return 1;
}

void CPioneerMobileDevice::Stop( void )
{
  speed = turnRate = 0;
}

void CPioneerMobileDevice::StoreRect( void )
{
  memcpy( &oldRect, &rect, sizeof( struct Rect ) );
  oldCenterx = centerx;
  oldCentery = centery;
}

void CPioneerMobileDevice::CalculateRect( float x, float y, float a )
{
  // fill an array of Points with the corners of the robots new position
  float cosa = cos( a );
  float sina = sin( a );
  float cxcosa = halfLength * cosa;
  float cycosa = halfWidth  * cosa;
  float cxsina = halfLength * sina;
  float cysina = halfWidth  * sina;

  rect.toplx = (int)(x + (-cxcosa + cysina));
  rect.toply = (int)(y + (-cxsina - cycosa));
  rect.toprx = (int)(x + (+cxcosa + cysina));
  rect.topry = (int)(y + (+cxsina - cycosa));
  rect.botlx = (int)(x + (+cxcosa - cysina));
  rect.botly = (int)(y + (+cxsina + cycosa));
  rect.botrx = (int)(x + (-cxcosa - cysina));
  rect.botry = (int)(y + (-cxsina + cycosa));

  centerx = (int)x;
  centery = (int)y;
}

void CPioneerMobileDevice::ParseCommandBuffer()
{
  // this will parse the P2OS command buffer to find out what to do

  //printf( "cmds: %d   ---    $s \n" , commands, commands );

  // read the command buffer and set the robot's speeds
  short v = *(short*)( commands );
  short w = *(short*)( commands + 2 );

  float fv = (float)(short)ntohs(v);//1000.0 * (float)ntohs(v);
  float fw = (float)(short)ntohs(w);//M_PI/180) * (float)ntohs(w);

  // set speeds unless we're being dragged
  if( m_world->win->dragging != m_robot )
    {
      speed = 0.001 * fv;
      turnRate = -(M_PI/180.0) * fw;
    }
  
  //cout << "DONE PARSE" << endl;
}

bool CPioneerMobileDevice::GUIUnDraw()
{
  m_world->win->SetForeground( 0 );
  m_world->win->DrawLines( undrawPts, 7 );

  return true;
}

bool CPioneerMobileDevice::GUIDraw()
{
  CWorldWin* win = m_world->win;
  
  XPoint pts[7];
  
  // draw the new position
  pts[4].x = pts[0].x = (short)rect.toprx;
  pts[4].y = pts[0].y = (short)rect.topry;
  pts[1].x = (short)rect.toplx;
  pts[1].y = (short)rect.toply;
  pts[6].x = pts[3].x = (short)rect.botlx;
  pts[6].y = pts[3].y = (short)rect.botly;
  pts[2].x = (short)rect.botrx;
  pts[2].y = (short)rect.botry;
  pts[5].x = (short)centerx;
  pts[5].y = (short)centery;
      
  win->SetForeground( win->RobotDrawColor( m_robot) );
  win->DrawLines( pts, 7 );

  // store these points for undrawing
  memcpy( undrawPts, pts, sizeof( XPoint ) * 7 ); 

  return true;
}




