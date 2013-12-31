/***************************************************************************
 *            qimagedisplay.cpp
 *
 *  Mon Jun 16 23:47:41 2008
 *  Copyright  2007  Christophe Seyve 
 *  Email cseyve@free.fr
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
#include "qimagedisplay.h"
#include <qevent.h>
#include <QPainter>

#include <stdio.h>

unsigned char g_debug_QImageDisplay = 0;

QImageDisplay::QImageDisplay(QWidget * l_parent)
	: QLabel(l_parent) {
	setMouseTracking(false);
	m_showVector = true;

	memset(&m_correction, 0, sizeof(t_correction));
}

void QImageDisplay::paintEvent(QPaintEvent * e)
{
	QRect cr;
	if(!e) { // repaint : redraw all
		cr = rect();
	} else {
		cr = e->rect();
	}

	if(pixmap()) {
//		fprintf(stderr, "QImageDisplay::%s:%d : e=%p cr=%d,%d+%dx%d pixmap=%dx%d\n",
//				__func__, __LINE__, e,
//				cr.x(), cr.y(), cr.width(), cr.height(),
//				pixmap()->width(), pixmap()->height()
//				);
		QImage ImgRGB = pixmap()->toImage();

		int xoff = (rect().width() - pixmap()->width())/2;
		int yoff = (rect().height() - pixmap()->height())/2;
		QPixmap pix(cr.size());

		pix.fill( this, cr.topLeft() );

		QPainter p(this); //( &pix);
		p.setRenderHint(QPainter::Antialiasing);

		if(cr != this->rect()) {
			p.drawImage(xoff, yoff, ImgRGB.copy(cr));
		} else {
			p.drawImage(xoff, yoff, ImgRGB);
		}

		if(m_showVector && m_correction.crop_width > 0)
		{
			// + 1 pixel offset for drawing
			int dest_x = m_correction.rel_dest_x - cr.x() ;
			int dest_y = m_correction.rel_dest_y - cr.y() ;
			int src_x = m_correction.rel_src_x - cr.x() ;
			int src_y = m_correction.rel_src_y - cr.y() ;

			// Offset for black arrow
			int off_x = 1, off_y = 1;
			if(dest_x < src_x) {
				off_x = -1;
			}
			if(dest_y < src_y) {
				off_y = -1;
			}
			src_x += off_x; src_y += off_y;
			dest_x += off_x; dest_y += off_y;

			// Gray brush for shadow, white for
			QBrush grayBrush(qRgba(0,0,0,20));
			QBrush whiteBrush(qRgba(255,255,255,180));
			p.setPen( QPen(grayBrush, 2));
			for(int pass=0; pass<2; pass++) {
				if(pass==1) {
					p.setPen( QPen(whiteBrush, 2));
					src_x -= off_x; src_y -= off_y;
					dest_x -= off_x; dest_y -= off_y;
				}

				int copy_vector_x = dest_x - src_x;
				int copy_vector_y = dest_y - src_y;
				int src_center_x = src_x;
				int src_center_y = src_y;
				int dest_center_x = dest_x;
				int dest_center_y = dest_y;

				p.drawEllipse( QPoint( src_x, src_y),
							m_correction.copy_width/2+1, m_correction.copy_height/2+1);
				// Copy vector
				float vector_len = sqrtf(copy_vector_x*copy_vector_x+copy_vector_y*copy_vector_y);
				if(vector_len > 10.f) {
					float shorter_len = vector_len - 10.f;
					if(shorter_len < 8.f) {
						shorter_len = 8.f;
					}

					copy_vector_x = (int)roundf((float)copy_vector_x * shorter_len / vector_len);
					copy_vector_y = (int)roundf((float)copy_vector_y * shorter_len / vector_len);
				}

				// Main move
				p.drawLine(src_center_x, src_center_y,
						   src_center_x + copy_vector_x, src_center_y + copy_vector_y
						   );

				int arrow_vector_x = copy_vector_x;
				int arrow_vector_y = copy_vector_y;

				// Arrow
				p.drawLine(src_center_x + arrow_vector_x*3/4 - arrow_vector_y*1/4 ,
						   src_center_y + arrow_vector_y*3/4 + arrow_vector_x*1/4 ,
						   src_center_x + copy_vector_x, src_center_y + copy_vector_y);
				p.drawLine(src_center_x + arrow_vector_x*3/4 + arrow_vector_y*1/4 ,
						   src_center_y + arrow_vector_y*3/4 - arrow_vector_x*1/4 ,
						   src_center_x + copy_vector_x, src_center_y + copy_vector_y);
			}
		}
		p.end();
		//bitBlt( this, cr.topLeft(), &pix );
	}

}

void QImageDisplay::focusInEvent ( QFocusEvent * e ) {

	if(g_debug_QImageDisplay)
		fprintf(stderr, "QImageDisplay::%s:%d : e=%p\n", __func__, __LINE__, e);

	emit signalFocusInEvent(e);
}
void QImageDisplay::focusOutEvent ( QFocusEvent * e ) {

	if(g_debug_QImageDisplay)
		fprintf(stderr, "QImageDisplay::%s:%d : e=%p\n", __func__, __LINE__, e);

	emit signalFocusOutEvent(e);
}

void QImageDisplay::mousePressEvent(QMouseEvent * e)
{
	if(g_debug_QImageDisplay)
		fprintf(stderr, "QImageDisplay::%s:%d : e=%p = %d,%d\n", __func__, __LINE__, e,
				e->x(), e->y());
//		fprintf(stderr, "QImageDisplay::%s:%d : e=%p\n", __func__, __LINE__, e);
	emit signalMousePressEvent(e);
}

void QImageDisplay::mouseReleaseEvent(QMouseEvent * e)
{
	if(g_debug_QImageDisplay)
			fprintf(stderr, "QImageDisplay::%s:%d : e=%p\n", __func__, __LINE__, e);
	emit signalMouseReleaseEvent(e);
}

void QImageDisplay::mouseMoveEvent(QMouseEvent * e)
{
	if(g_debug_QImageDisplay)
		fprintf(stderr, "QImageDisplay::%s:%d : e=%p = %d,%d\n", __func__, __LINE__, e,
				e->x(), e->y());
	emit signalMouseMoveEvent(e);
}

void QImageDisplay::wheelEvent(QWheelEvent * e)
{
	if(e) {
		int numDegrees = e->delta() / 8;
		int numSteps = numDegrees / 15;

		if(g_debug_QImageDisplay) {
			fprintf(stderr, "QImageDisplay::%s:%d : Wheel event e=%p delta=%d\n",
					__func__, __LINE__, e, e->delta());
			fprintf(stderr, "\tDeplacement :\t%d steps = %d degrees\n",
					numSteps, numDegrees );
			fprintf(stderr, "\tOrientation :\t");
			switch(e->orientation()) {
			case Qt::Horizontal:
				fprintf(stderr, "Qt::Horizontal\n");
				break;
			case Qt::Vertical:
				fprintf(stderr, "Qt::Vertical\n");
				break;
			default:
				fprintf(stderr, "Unknown\n");
				break;
			}
		}
	}
	
	emit signalWheelEvent(e);
}
