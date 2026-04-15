#include "glut.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef WIN32
#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")
#endif

/*
** ステレオ表示の選択
*/
#define NONE 0
#define QUADBUF 1
#define BARRIER 2
#define STEREO BARRIER

/*
** ディスプレイサイズの選択
*/
#define CRT17 0
#define VRROOM 1
#define DISPSIZE CRT17

/*
** ヘッドトラッキングするかどうか（Onyxのみ）
*/
#define TRACKING 0

/*
** ゲームモードを使うかどうか
*/
#define GAMEMODE 0

/*
** 視差バリア用の画像を生成する際のマスクに使う
** ステンシルバッファのビット
*/
#define BARRIERBIT 1

/*
** 飛んでくる玉の最大数
*/
#define MAXBALL 100

/*
** 環境設定
*/
#define PX 0.0         /* 初期位置　　　　　　　　　　 */
#define PY 1.5         /* 初期位置　　　　　　　　　　 */
#define PZ (-5.0)      /* 初期位置　　　　　　　　　　 */
#define TIMESCALE 0.01 /* フレームごとの時間　　　　　 */
#define G -9.8         /* 重力加速度　　　　　　　　　 */
#define SPEED 25.0     /* 球スピード　　　　　　　　　 */
#define PI 0.3         /* 投球範囲　　　　　　　　　　 */

#define zNear 0.3 /* 前方面の位置　　　　　　　　 */
#define zFar 20.0 /* 後方面の位置　　　　　　　　 */

#if DISPSIZE == VRROOM
#define W 6.0
#define H 1.5
#define D 2.0
#define P 0.06
#elif DISPSIZE == CRT17
#define W 2.0
#define H 1.5
#define D 2.0
#define P 0.06
#endif

/*
** ヘッドトラッキングに使う関数（Onyxのみ）
*/
#if TRACKING
extern int startIsoTrak(char *);
extern void stopIsoTrak(int);
extern float *getIsoTrak(int);
int fd, lock = 0;
#endif

/*
** 球のデータ
*/
struct balllist {
  double vx, vy, vz; /* 初速度　　　　　　　　　　　 */
  double px, py, pz; /* 現在位置　　　　　　　　　　 */
  int c;             /* 色番号　　　　　　　　　　　 */
  struct balllist *p, *n;
} start, stop, *reserve;

float bx, by, bz;    /* ヘッドトラッキングの基準位置 */
int cx, cy;          /* ウィンドウの中心　　　　　　 */
int ballcount = 0;   /* ばら撒かれているボールの数　 */
double parallax = P; /* 視差　　　　　　　　　　　　 */

/*
 * 地面
 */
void myGround(double height) {
  static GLfloat ground[][4] = {{0.6, 0.6, 0.6, 1.0}, {0.3, 0.3, 0.3, 1.0}};

  int i, j;

  glBegin(GL_QUADS);
  glNormal3d(0.0, 1.0, 0.0);
  for (j = -10; j <= 10; j++) {
    for (i = -10; i < 10; i++) {
      glMaterialfv(GL_FRONT, GL_DIFFUSE, ground[(i + j) & 1]);
      glVertex3d((GLdouble)i, height, (GLdouble)j);
      glVertex3d((GLdouble)i, height, (GLdouble)(j + 1));
      glVertex3d((GLdouble)(i + 1), height, (GLdouble)(j + 1));
      glVertex3d((GLdouble)(i + 1), height, (GLdouble)j);
    }
  }
  glEnd();
}

/*
 * シーンの描画
 */
void scene(void) {
  static GLfloat ballColor[][4] = {
      /* 球の色 */
      {0.2, 0.2, 0.2, 1.0}, {0.2, 0.2, 0.8, 1.0}, {0.2, 0.8, 0.2, 1.0},
      {0.2, 0.8, 0.8, 1.0}, {0.8, 0.2, 0.2, 1.0}, {0.8, 0.2, 0.8, 1.0},
      {0.8, 0.8, 0.2, 1.0}, {0.8, 0.8, 0.8, 1.0},
  };
  struct balllist *p;

  myGround(0.0);
  glPushMatrix();
  glMaterialfv(GL_FRONT, GL_DIFFUSE, ballColor[0]);
  glTranslated(PX, PY, PZ);
  glutSolidCube(0.5);
  glPopMatrix();

  for (p = start.n; p != &stop; p = p->n) {
    glPushMatrix();
    glTranslated(p->px, p->py, p->pz);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, ballColor[p->c]);
    glutSolidSphere(0.1, 16, 8);
    glPopMatrix();
  }
}

/*
 * 球の放出
 */
int shot(int value) {
  int count = 0;

  while (reserve && --value >= 0) {
    double u, v;
    struct balllist *p;

    p = reserve;
    reserve = reserve->n;

    u = (2.0 * (double)rand() / (double)RAND_MAX - 1.0) * PI;
    v = (2.0 * (double)rand() / (double)RAND_MAX - 1.0) * PI;

    p->vx = SPEED * sin(u) * cos(v);
    p->vy = SPEED * sin(v);
    p->vz = SPEED * cos(u) * cos(v);

    p->px = PX;
    p->py = PY;
    p->pz = PZ;

    (start.n = (p->n = start.n)->p = p)->p = &start;

    ++count;
  }

  return count;
}

/*
 * シーンの更新
 */
void update(void) {
  struct balllist *p, *n;

  for (p = start.n; p != &stop; p = n) {
    n = p->n;

    p->px += p->vx * TIMESCALE;
    p->vy += G * TIMESCALE;
    p->py += p->vy * TIMESCALE;
    p->pz += p->vz * TIMESCALE;

    if (p->px < -5.0 || p->px > 5.0 || p->py > 5.0 || p->pz > 20.0) {
      /* ステージから飛び出た球のデータは削除・回収 */
      p->n->p = p->p;
      p->p->n = p->n;
      p->n = reserve;
      reserve = p;

      if (ballcount > 0)
        shot(1);
    } else if (p->py < 0.1) {
      /* 安直な跳ね返り計算 */
      p->vy = -p->vy * 0.8;
    }
  }
}

/*
 * 画面表示
 */
void display(void) {
  static GLfloat lightpos[] = {3.0, 4.0, 5.0, 0.0}; /* 光源の位置 */

  GLdouble k = 0.5 * zNear / D;
  GLdouble f = parallax * k;
  GLdouble w = W * k;
  GLdouble h = H * k;

  float ex, ey, ez;

#if TRACKING
  if (lock) {
    float *handle = getIsoTrak(0);

    if (handle) {
      ex = (handle[0] - bx) * 0.02;
      ey = (by - handle[1]) * 0.02;
      ez = (handle[2] - bz) * 0.02;
    } else {
      ex = ey = ez = 0.0;
    }
  } else {
    ex = ey = ez = 0.0;
  }
#else
  ex = ey = ez = 0.0;
#endif

  /* 右目の画像 */
#if STEREO == QUADBUF
  glDrawBuffer(GL_BACK_RIGHT);
#elif STEREO == BARRIER
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
#endif
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

#if STEREO != NONE
  glFrustum(-w - f, w - f, -h, h, zNear, zFar);
#else
  glFrustum(-w, w, -h, h, zNear, zFar);
#endif

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslated(-parallax * 0.5, 0.0, -D);

  /* 視点の移動（物体の方を奥に移す）*/
  glTranslated(ex, ey - PY, ez - 10.0);

  /* 光源の位置を設定 */
  glLightfv(GL_LIGHT0, GL_POSITION, lightpos);

#if STEREO == BARRIER
  /* 奇数ラインにＲ・Ｂを表示 */
  glStencilFunc(GL_NOTEQUAL, BARRIERBIT, BARRIERBIT);
  glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_TRUE);
  glNewList(1, GL_COMPILE_AND_EXECUTE);
#endif
  /* シーンの描画 */
  scene();
#if STEREO == BARRIER
  glEndList();
  /* 偶数ラインにＧを表示 */
  glStencilFunc(GL_EQUAL, BARRIERBIT, BARRIERBIT);
  glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
  /* シーンの描画 */
  glCallList(1);
#endif

#if STEREO != NONE

  /* 左目の画像 */
#if STEREO == QUADBUF
  glDrawBuffer(GL_BACK_LEFT);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#endif

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-w + f, w + f, -h, h, zNear, zFar);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslated(parallax * 0.5, 0.0, -D);

  /* 視点の移動（物体の方を奥に移す）*/
  glTranslated(ex, ey - PY, ez - 10.0);

  /* 光源の位置を設定 */
  glLightfv(GL_LIGHT0, GL_POSITION, lightpos);

#if STEREO == BARRIER
  /* 偶数ラインにＲ・Ｂを表示 */
  glStencilFunc(GL_EQUAL, BARRIERBIT, BARRIERBIT);
  glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_TRUE);
  glClear(GL_DEPTH_BUFFER_BIT);
#endif
  /* シーンの描画 */
  glCallList(1);
#if STEREO == BARRIER
  /* 奇数ラインにＧを表示 */
  glStencilFunc(GL_NOTEQUAL, BARRIERBIT, BARRIERBIT);
  glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
  /* シーンの描画 */
  glCallList(1);
#endif

#endif

  glutSwapBuffers();

  /* シーンの更新 */
  update();
}

/*
 * 画面の初期化
 */
void resize(int w, int h) {
  int x;

  /* ウィンドウの中心 */
  cx = w / 2;
  cy = h / 2;

  /* ウィンドウ全体をビューポートにする */
  glViewport(0, 0, w, h);

  /* ステンシルバッファにマスクを描く */
#if STEREO == BARRIER
  glClearStencil(0);
  glStencilFunc(GL_ALWAYS, BARRIERBIT, BARRIERBIT);
  glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
  glDisable(GL_DEPTH_TEST);
  glDrawBuffer(GL_NONE);
  glClear(GL_STENCIL_BUFFER_BIT);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-0.5, (GLdouble)w, -0.5, (GLdouble)h, -1.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glBegin(GL_LINES);
  for (x = 0; x < w; x += 2) {
    glVertex2d(x, 0);
    glVertex2d(x, h - 1);
  }
  glEnd();
  glFlush();
  glDrawBuffer(GL_BACK);
  glEnable(GL_DEPTH_TEST);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
#endif
}

void keyboard(unsigned char key, int x, int y) {
  /* ESC か q をタイプしたら終了 */
  if (key == '\033' || key == 'q') {
#if TRACKING
    stopIsoTrak(fd);
#endif
#if GAMEMODE
    glutLeaveGameMode();
#endif
    exit(0);
  } else if (key == ' ') {
    ballcount += shot(1);
  } else if (key == 'o') {
    parallax += 0.05;
  } else if (key == 'c') {
    parallax -= 0.05;
  }
#if TRACKING
  else if (key == 's') {
    if (lock = 1 - lock) {
      float *handle = getIsoTrak(0);

      if (handle) {
        bx = handle[0];
        by = handle[1];
        bz = handle[2];
      }
    }
  }
#endif
}

void idle(void) { glutPostRedisplay(); }

void mouse(int button, int state, int x, int y) {
  if (state == GLUT_DOWN) {
    switch (button) {
    case GLUT_LEFT_BUTTON:
      if (reserve) {
        double u, v;
        struct balllist *p;

        p = reserve;
        reserve = reserve->n;

        u = (double)(x - cx) * PI / (double)cx;
        v = (double)(cy - y) * PI / (double)cy;

        p->vx = SPEED * sin(u) * cos(v);
        p->vy = SPEED * sin(v);
        p->vz = SPEED * cos(u) * cos(v);

        p->px = PX;
        p->py = PY;
        p->pz = PZ;

        (start.n = (p->n = start.n)->p = p)->p = &start;
      }
      break;
    case GLUT_MIDDLE_BUTTON:
      glutIdleFunc(0);
      break;
    case GLUT_RIGHT_BUTTON:
      glutPostRedisplay();
      break;
    default:
      break;
    }
  }
}

void init(void) {
  struct balllist *p;
  int n = MAXBALL;

  start.n = &stop;
  start.p = 0;
  stop.n = 0;
  stop.p = &start;
  reserve = p = malloc(sizeof(struct balllist) * MAXBALL);

  while (--n > 0) {
    p->c = n % 8;
    p->n = p + 1;
    ++p;
  }
  p->c = 7;
  p->n = 0;

  /* 初期設定 */
  glClearColor(1.0, 1.0, 1.0, 0.0);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);

#if STEREO == BARRIER
  glEnable(GL_STENCIL_TEST);
#endif
}

int main(int argc, char *argv[]) {
#if TRACKING
  glutInitWindowPosition(0, 600);
  glutInitWindowSize(1920, 480);
#endif
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_RGBA | GLUT_DEPTH | GLUT_DOUBLE
#if STEREO == QUADBUF
                      | GLUT_STEREO
#elif STEREO == BARRIER
                      | GLUT_STENCIL
#endif
  );
#if GAMEMODE
  glutGameModeString("width=1024 height=768 bpp~24 hertz=100");
  glutEnterGameMode();
#else
  glutCreateWindow(argv[0]);
#if STEREO == BARRIER
  glutFullScreen();
#endif
#endif
  glutDisplayFunc(display);
  glutReshapeFunc(resize);
  glutKeyboardFunc(keyboard);
  glutMouseFunc(mouse);
  glutIdleFunc(idle);
  init();
#if TRACKING
  fd = startIsoTrak("/dev/ttyd5");
#endif
  glutMainLoop();
  return 0;
}
