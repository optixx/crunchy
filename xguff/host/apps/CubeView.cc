//
// "$Id: CubeView.cxx 4288 2005-04-16 00:13:17Z mike $"
//
// CubeView class implementation for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2005 by Bill Spitzak and others.
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA.
//
// Please report all bugs and problems on the following page:
//
//     http://www.fltk.org/str.php
//

#include "CubeView.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <Fl/gl.h>
#include "rfile_util.h"

#if HAVE_GL
CubeView::CubeView(int x,int y,int w,int h,const char *l)
            : Fl_Gl_Window(x,y,w,h,l)
#else
CubeView::CubeView(int x,int y,int w,int h,const char *l)
            : Fl_Box(x,y,w,h,l)
#endif /* HAVE_GL */
{
    vAng = 0.0;
    hAng=0.0;
    size=0.0;
	xshift=0.0;
	yshift=0.0;
    
    /* The cube definition. These are the vertices of a unit cube
     * centered on the origin.*/
    
    boxv0[0] = -0.5; boxv0[1] = -0.5; boxv0[2] = -0.5;
    boxv1[0] =  0.5; boxv1[1] = -0.5; boxv1[2] = -0.5;
    boxv2[0] =  0.5; boxv2[1] =  0.5; boxv2[2] = -0.5;
    boxv3[0] = -0.5; boxv3[1] =  0.5; boxv3[2] = -0.5;
    boxv4[0] = -0.5; boxv4[1] = -0.5; boxv4[2] =  0.5;
    boxv5[0] =  0.5; boxv5[1] = -0.5; boxv5[2] =  0.5;
    boxv6[0] =  0.5; boxv6[1] =  0.5; boxv6[2] =  0.5;
    boxv7[0] = -0.5; boxv7[1] =  0.5; boxv7[2] =  0.5;

    for (unsigned d=0; d<5; d++) xdata_p[d] = NULL;

#if !HAVE_GL
    label("OpenGL is required for this demo to operate.");
    align(FL_ALIGN_WRAP | FL_ALIGN_INSIDE);
#endif /* !HAVE_GL */
}

#if HAVE_GL

void CubeView::drawPlot() {
    glShadeModel(GL_FLAT);
    glBegin(GL_LINES);
    float v[3];
    if (0) fprintf(stderr,"CubeView::drawPlot npt=%lu\n", npt);
    double *xdata = xdata_p[0];
    for (unsigned int d=0; d<2; d++) {
        if (d==0) glColor3f(0.9, 0.4, 0.9);
	     else glColor3f(0.4, 0.9, 0.9);
    	double *yrdata = xdata_p[2*d+1];
	double *yidata = xdata_p[2*d+2];
    	for (unsigned long u=0; u<npt; u++) {
    		if (0) fprintf(stderr,"display x[%lu]=%f y[%lu]=%f\n", u, xdata[u], u, yrdata[u]);
		v[0] = xdata[u];
		v[1] = yrdata[u];
		v[2] = yidata[u];
		glVertex3fv(v);
		if (u!=0 && u+1!=npt) glVertex3fv(v);
    	}
    }
    /* crude substitute for axes */
    glColor3f(0.4, 0.4, 0.4);
    v[0] = 0.0;
    v[1] = 0.0;
    v[2] = 0.0;
    glVertex3fv(v);
    double rd=100.0;
    double minrd = pow(0.1,.05*size)*6.0;
    double maxrd = pow(0.1,.05*size)*150.0;
    for (unsigned int rs=0; rs<=15; rs++) {
        if (rd > minrd && rd <= maxrd) {
    	    for (unsigned int u=0; u<=13; u++) {
   	        v[1] = rd * sin(u*2.0*3.1415926/13.0);
	        v[2] = rd * cos(u*2.0*3.1415926/13.0);
	        glVertex3fv(v);
	        glVertex3fv(v);
	    }
    	}
	rd = rd * 0.50119;
    }
    v[1] = 0.0;
    v[2] = 0.0;
    glVertex3fv(v);
    glVertex3fv(v);
    v[0] = 1.0;
    glVertex3fv(v);
    glEnd();

#if 1
    gl_font(FL_HELVETICA_BOLD,16);
    glDisable(GL_DEPTH_TEST);
    char axis_label[30];
    sprintf(axis_label,"%.2f ms", tscale*npt*1000);
    gl_draw(axis_label, 1.0f, 0.0f);
    rd=100.0;
    for (unsigned int rs=0; rs<=15; rs++) {
	if (rd > minrd && rd <= maxrd) {
	    sprintf(axis_label,"-%d dB",rs*6);
	    gl_draw(axis_label, 0.0f, (float) rd);
	}
	rd = rd * 0.50119;
    }
    // glRasterPos3d(0.0, 0.354, 0.354); gl_draw("foo");
    glEnable(GL_DEPTH_TEST);
#endif

}

void CubeView::drawCube() {
/* Draw a colored cube */
#define ALPHA 0.5
    glShadeModel(GL_FLAT);

    glBegin(GL_QUADS);
      glColor4f(0.0, 0.0, 1.0, ALPHA);
      glVertex3fv(boxv0);
      glVertex3fv(boxv1);
      glVertex3fv(boxv2);
      glVertex3fv(boxv3);

      glColor4f(1.0, 1.0, 0.0, ALPHA);
      glVertex3fv(boxv0);
      glVertex3fv(boxv4);
      glVertex3fv(boxv5);
      glVertex3fv(boxv1);

      glColor4f(0.0, 1.0, 1.0, ALPHA);
      glVertex3fv(boxv2);
      glVertex3fv(boxv6);
      glVertex3fv(boxv7);
      glVertex3fv(boxv3);

      glColor4f(1.0, 0.0, 0.0, ALPHA);
      glVertex3fv(boxv4);
      glVertex3fv(boxv5);
      glVertex3fv(boxv6);
      glVertex3fv(boxv7);

      glColor4f(1.0, 0.0, 1.0, ALPHA);
      glVertex3fv(boxv0);
      glVertex3fv(boxv3);
      glVertex3fv(boxv7);
      glVertex3fv(boxv4);

      glColor4f(0.0, 1.0, 0.0, ALPHA);
      glVertex3fv(boxv1);
      glVertex3fv(boxv5);
      glVertex3fv(boxv6);
      glVertex3fv(boxv2);
    glEnd();

    glColor3f(1.0, 1.0, 1.0);
    glBegin(GL_LINES);
      glVertex3fv(boxv0);
      glVertex3fv(boxv1);

      glVertex3fv(boxv1);
      glVertex3fv(boxv2);

      glVertex3fv(boxv2);
      glVertex3fv(boxv3);

      glVertex3fv(boxv3);
      glVertex3fv(boxv0);

      glVertex3fv(boxv4);
      glVertex3fv(boxv5);

      glVertex3fv(boxv5);
      glVertex3fv(boxv6);

      glVertex3fv(boxv6);
      glVertex3fv(boxv7);

      glVertex3fv(boxv7);
      glVertex3fv(boxv4);

      glVertex3fv(boxv0);
      glVertex3fv(boxv4);

      glVertex3fv(boxv1);
      glVertex3fv(boxv5);

      glVertex3fv(boxv2);
      glVertex3fv(boxv6);

      glVertex3fv(boxv3);
      glVertex3fv(boxv7);
    glEnd();
}//drawCube

void CubeView::draw() {
    if (!valid()) {
        glLoadIdentity();
        glViewport(0,0,w(),h());
        glOrtho(-10,10,-10,10,-20050,10000);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glPushMatrix();

    glTranslatef(xshift, yshift, 0);
    glRotatef(hAng,0,1,0); glRotatef(vAng,1,0,0);
    // interpret size as dB
    float scalef = pow(10.0,0.05*size)*10*0.01;  // XXX the 0.01 is hack, see matching hack in rfile_util.c
    glScalef(10.0,scalef,scalef);

    drawPlot();
    
    glPopMatrix();
}
#endif /* HAVE_GL */

void CubeView::set_file(const char *fname)
{
#if 0
	char buff[1024];
	if (0) printf("CubeView::set_file (%s)\n", fname);
	FILE *f = fopen(fname,"r");
	if (!f) {
		perror(fname);
		return;
	}
	if (fgets(buff,sizeof buff, f)==NULL) {
		fprintf(stderr,"nothing useful in %s\n",fname);
		return;
	}
	npt = strtoul(buff,NULL,0);
	if (0) fprintf(stderr, "CubeView::set_file npt = %lu\n", npt);
	xdata=(double *) realloc(xdata,npt*sizeof *xdata);
	ydata=(double *) realloc(ydata,npt*sizeof *ydata);
	if (xdata == NULL || ydata == NULL) {
		perror("realloc");
		goto fail;
	}
	for (unsigned long u=0; u<npt; u++) {
		char *p1, *p2;
		if (fgets(buff,sizeof buff, f)==NULL) {
			fprintf(stderr,"error reading data point %lu\n", u);
			goto fail;
		}
		if (0) fprintf(stderr, "line %lu = %s", npt, buff);
		if ((xdata[u] = strtod(buff, &p1)),(p1 != buff) &&
		    (ydata[u] = strtod(p1,   &p2)),(p2 != p1)) {
			/* success */
		} else {
			fprintf(stderr,"error interpreting line %lu (%s)\n", u, buff);
			goto fail;
		}
	}
    fail:
	fclose(f);
#else 
	if (llrf_read_header(fname,0)==0) {
		npt = llrf_fill_data(xdata_p);
		tscale = llrf_tscale();
		llrf_unmap();
	}
#endif
	// fprintf(stderr, "# CubeView::set_file (%s) read %lu points\n", fname, npt);
	redraw();
}

//
// End of "$Id: CubeView.cxx 4288 2005-04-16 00:13:17Z mike $".
//
