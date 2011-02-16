// Copyright 2011 Jason Marcell

#include <ctype.h>

#include <gsl/gsl_statistics.h>

#include "lib/Matrix.h"

namespace jason {

Matrix::Matrix(size_t height, size_t width) {
  this->m = gsl_matrix_alloc(height, width);
}

Matrix::Matrix(gsl_matrix *mat) {
  this->m = mat;
}

Matrix::Matrix(Vector *vec) {
  gsl_matrix *mat = gsl_matrix_alloc(vec->v->size, vec->v->size);
  gsl_vector_view diag = gsl_matrix_diagonal(mat);
  gsl_matrix_set_all(mat, 0.0);
  gsl_vector_memcpy(&diag.vector, vec->v);
  this->m = mat;
}

Matrix::Matrix(double *data, size_t height, size_t width) {
  this->m = gsl_matrix_alloc(height, width);
  for (size_t row = 0; row < height; ++row) {
    for (size_t col = 0; col < width; ++col) {
      gsl_matrix_set(m, row, col, *(data + row * width + col));
    }
  }
}

Matrix::Matrix(const char* filename) {
  int rows, cols;
  FILE *f;
  f = fopen(filename, "r");
  if (f) {
    rows = NumberOfRows(f);
    cols = NumberOfColumns(f);
    this->m = gsl_matrix_alloc(rows, cols);
    gsl_matrix_fscanf(f, this->m);
    fclose(f);
  } else {
    perror("Error");
    throw("File read error.");
  }
}

Matrix::~Matrix() {
  gsl_matrix_free(this->m);
}

Matrix *Matrix::Clone() {
  return new Matrix(this->m->data, this->Width(), this->Height());
}

Matrix *Matrix::Invert() {
  int n = this->Width();
  gsl_matrix *copy = this->CloneGSLMatrix();
  gsl_matrix *inverse = gsl_matrix_alloc(n, n);
  gsl_permutation *perm = gsl_permutation_alloc(n);
  int s = 0;
  gsl_linalg_LU_decomp(copy, perm, &s);
  gsl_linalg_LU_invert(copy, perm, inverse);
  gsl_permutation_free(perm);
  gsl_matrix_free(copy);
  return new Matrix(inverse->data, n, n);
}

double Matrix::Get(int row, int col) {
  return gsl_matrix_get(this->m, row, col);
}

void Matrix::Set(int row, int col, double val) {
  gsl_matrix_set(this->m, row, col, val);
}

void Matrix::Add(Matrix *other) {
  gsl_matrix_add(this->m, other->m);
}

Vector* Matrix::Row(size_t row) {
  gsl_vector *v = gsl_vector_alloc(this->Width());
  gsl_matrix_get_row(v, m, row);
  return new Vector(v);
}

Vector* Matrix::Column(size_t col) {
  gsl_vector *v = gsl_vector_alloc(this->Height());
  gsl_matrix_get_col(v, m, col);
  return new Vector(v);
}

void Matrix::SetRow(size_t row, Vector *vec) {
  gsl_matrix_set_row(m, row, vec->v);
}

void Matrix::SetColumn(size_t col, Vector *vec) {
  gsl_matrix_set_col(m, col, vec->v);
}

void Matrix::Sphere() {
  size_t height = this->Height();
  size_t width = this->Width();
  Vector *vec;
  for (size_t col = 0; col < width; ++col) {
    double sum = 0.0;
    for (size_t row = 0; row < height; ++row) {
      sum += this->Get(row, col);
    }
    vec = this->Column(col);
    double mean = gsl_stats_mean(vec->v->data, 1, height);
    double stdev = gsl_stats_sd(vec->v->data, 1, height);
    gsl_vector_add_constant(vec->v, -mean);
    gsl_vector_scale(vec->v, 1.0 / stdev);
    gsl_matrix_set_col(m, col, vec->v);
    delete vec;
  }
}

void Matrix::Print() {  // TODO(jrm): turn into ToString
  for (size_t row = 0; row < this->Height(); ++row) {
    for (size_t col = 0; col < this->Width(); ++col) {
      printf("%.2f ", this->Get(row, col));
    }
    printf("\n");
  }
}

size_t Matrix::Height() {
  return this->m->size1;
}

size_t Matrix::Width() {
  return this->m->size2;
}

Matrix* Matrix::Multiply(Matrix *other) {
  gsl_matrix *result = gsl_matrix_alloc(this->Height(), other->Height());
  cblas_dgemm(CblasRowMajor,  // const enum CBLAS_ORDER Order
      CblasNoTrans,           // const enum CBLAS_TRANSPOSE TransA
      CblasTrans,             // const enum CBLAS_TRANSPOSE TransB
      this->Height(),         // const int M
      other->Height(),        // const int N
      this->Width(),          // const int K
      1.0f,                   // const double alpha
      this->m->data,          // const double * A
      this->Width(),          // const int lda
      other->m->data,         // const double * B
      other->Width(),         // const int ldb
      0.0f,                   // const double beta
      result->data,           // double * C
      result->size2);         // const int ldc
  return new Matrix(result->data, this->Height(), other->Height());
}

Vector* Matrix::Multiply(Vector *vec) {
  gsl_vector *result = gsl_vector_calloc(vec->Size());
  cblas_dgemm(CblasRowMajor,  // const enum CBLAS_ORDER Order
      CblasNoTrans,           // const enum CBLAS_TRANSPOSE TransA
      CblasTrans,             // const enum CBLAS_TRANSPOSE TransB
      this->Height(),         // const int M (height of A)
      vec->Size(),            // const int N (width of B)
      this->Width(),          // const int K (width of A)
      1.0f,                   // const double alpha
      this->m->data,          // const double * A
      m->size2,               // const int lda
      vec->v->data,           // const double * B
      vec->Size(),            // const int ldb
      0.0f,                   // const double beta
      result->data,           // double * C
      1);                     // const int ldc
  return new Vector(result->data, result->size);
}

int Matrix::NumberOfRows(FILE *f) {
  char lastChar = '\n';
  char currentChar = NULL;
  int count = 0;
  while ((currentChar = fgetc(f)) != EOF) {
    if (lastChar == '\n' && currentChar != '\n') {
      ++count;
    }
    lastChar = currentChar;
  }
  fseek(f, 0, SEEK_SET);
  return count;
}

int Matrix::NumberOfColumns(FILE *f) {
  char lastChar = ' ';
  char currentChar = NULL;
  int count = 0;
  while ((currentChar = fgetc(f)) != '\n') {
    if (isspace(lastChar) && !isspace(currentChar)) {
      ++count;
    }
    lastChar = currentChar;
  }
  fseek(f, 0, SEEK_SET);
  return count;
}

gsl_matrix *Matrix::CloneGSLMatrix() {
  size_t height = this->Height();
  size_t width = this->Width();
  gsl_matrix *m = gsl_matrix_alloc(height, width);
  for (size_t row = 0; row < height; ++row) {
    for (size_t col = 0; col < width; ++col) {
      this->Set(row, col, *(this->m->data + row * width + col));
    }
  }
  return m;
}
}