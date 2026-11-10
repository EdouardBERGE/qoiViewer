int needsSharpening(unsigned int *buffer, int width, int height) {
  if (width < 3 || height < 3 || buffer == NULL) {
    return 0; // Image trop petite pour être traitée
  }

  int total_pixels = width * height;

  // Allocation des tampons temporaires pour les niveaux de gris et le Laplacien
  float *gray = (float *)malloc(total_pixels * sizeof(float));
  float *laplacian = (float *)malloc(total_pixels * sizeof(float));

  if (!gray || !laplacian) {
    if (gray)
      free(gray);
    if (laplacian)
      free(laplacian);
    return 0; // Sécurité en cas d'échec d'allocation mémoire
  }

  // 1. Conversion du buffer en nuances de gris (Luminance)
  // Hypothèse de format standard ARGB/RGBA (0xRRGGBB ou similaire).
  // Ajustez les décalages si votre format diffère (ex: BGRA).
  for (int i = 0; i < total_pixels; i++) {
    unsigned int p = buffer[i];
    float r = (float)((p >> 16) & 0xFF);
    float g = (float)((p >> 8) & 0xFF);
    float b = (float)(p & 0xFF);
    gray[i] = 0.299f * r + 0.587f * g + 0.114f * b;
  }

  // 2. Application du filtre Laplacien (Noyau 3x3)
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int idx = y * width + x;
      // Gestion des bords (on ignore le pourtour direct pour simplifier)
      if (x == 0 || x == width - 1 || y == 0 || y == height - 1) {
        laplacian[idx] = 0.0f;
      } else {
        float center = gray[idx];
        float top = gray[(y - 1) * width + x];
        float bottom = gray[(y + 1) * width + x];
        float left = gray[y * width + (x - 1)];
        float right = gray[y * width + (x + 1)];

        laplacian[idx] = top + bottom + left + right - 4.0f * center;
      }
    }
  }

  // 3. Calcul des moyennes pour les variances
  float sum_gray = 0.0f;
  float sum_lap = 0.0f;

  for (int i = 0; i < total_pixels; i++) {
    sum_gray += gray[i];
    sum_lap += laplacian[i];
  }
  float mean_gray = sum_gray / total_pixels;
  float mean_lap = sum_lap / total_pixels;

  // 4. Calcul des variances (Énergies globale et haute fréquence)
  float var_gray = 0.0f;
  float var_lap = 0.0f;

  for (int i = 0; i < total_pixels; i++) {
    float diff_g = gray[i] - mean_gray;
    var_gray += diff_g * diff_g;

    float diff_l = laplacian[i] - mean_lap;
    var_lap += diff_l * diff_l;
  }
  var_gray /= total_pixels;
  var_lap /= total_pixels;

  // Libération de la mémoire
  free(gray);
  free(laplacian);

  // 5. Évaluation finale (Division par zéro évitée si image unie)
  if (var_gray == 0.0f) {
    return 0;
  }

  float ratio = var_lap / var_gray;

  // Seuil de décision empirique (généralement entre 0.02 et 0.05)
  float threshold = 0.03f;

  // Retourne 1 si l'image est trop douce (nécessite un sharpen), 0 sinon
  return (ratio < threshold) ? 1 : 0;
}


void applySharpen( unsigned int *buffer, int width, int height, int w) {
  if (width < 3 || height < 3 || buffer == NULL) {
    return; // Image trop petite ou buffer invalide
  }

  int total_pixels = width * height;

  // Allocation d'un tampon temporaire pour lire les pixels originaux
  unsigned int *temp_buffer =
      (unsigned int *)malloc(total_pixels * sizeof(unsigned int));
  if (!temp_buffer) {
    return; // Échec d'allocation mémoire
  }

  // Copie de sauvegarde du buffer source
  for (int i = 0; i < total_pixels; i++) {
    temp_buffer[i] = buffer[i];
  }

  // Application du noyau de renforcement (Noyau 5 points)
  // [  0, -1,  0 ]
  // [ -1,  5, -1 ]
  // [  0, -1,  0 ]
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int idx = y * width + x;

      // Ignorer les bords pour simplifier (les pixels du contour restent intacts)
      if (x == 0 || x == width - 1 || y == 0 || y == height - 1) {
        continue;
      }

      unsigned int center = temp_buffer[idx];
      unsigned int top = temp_buffer[(y - 1) * width + x];
      unsigned int bottom = temp_buffer[(y + 1) * width + x];
      unsigned int left = temp_buffer[y * width + (x - 1)];
      unsigned int right = temp_buffer[y * width + (x + 1)];

      // Extraction des composantes RVB et Alpha du pixel central
      int rc = (center >> 16) & 0xFF;
      int gc = (center >> 8) & 0xFF;
      int bc = center & 0xFF;
      unsigned int alpha = center & 0xFF000000; // Préservation du canal Alpha

      // Extraction des composantes RVB des voisins
      int rt = (top >> 16) & 0xFF;
      int gt = (top >> 8) & 0xFF;
      int bt = top & 0xFF;

      int rb = (bottom >> 16) & 0xFF;
      int gb = (bottom >> 8) & 0xFF;
      int bb = bottom & 0xFF;

      int rl = (left >> 16) & 0xFF;
      int gl = (left >> 8) & 0xFF;
      int bl = left & 0xFF;

      int rr = (right >> 16) & 0xFF;
      int gr = (right >> 8) & 0xFF;
      int br = right & 0xFF;

      // Calcul du filtre pour chaque canal : 5 * C - (Top + Bottom + Left + Right)
      //int r_out = 5 * rc - rt - rb - rl - rr;
      //int g_out = 5 * gc - gt - gb - gl - gr;
      //int b_out = 5 * bc - bt - bb - bl - br;

      int r_out = rc + ((w * (4*rc - rt - rb - rl - rr) + 128) >> 8);
      int g_out = gc + ((w * (4*gc - gt - gb - gl - gr) + 128) >> 8);
      int b_out = bc + ((w * (4*bc - bt - bb - bl - br) + 128) >> 8);

      // Saturation (Clamping) entre 0 et 255 pour éviter les dépassements
      r_out = r_out < 0 ? 0 : (r_out > 255 ? 255 : r_out);
      g_out = g_out < 0 ? 0 : (g_out > 255 ? 255 : g_out);
      b_out = b_out < 0 ? 0 : (b_out > 255 ? 255 : b_out);

      // Réassemblage du pixel dans le buffer final
      buffer[idx] = alpha | ((unsigned int)r_out << 16) |
                    ((unsigned int)g_out << 8) | (unsigned int)b_out;
    }
  }

  free(temp_buffer);
}

#include <stdlib.h>

void applyVerticalSharpen( unsigned int *buffer, int width, int height) {
  if (width <= 0 || height < 3 || buffer == NULL) {
    return;
  }

  int total_pixels = width * height;
  unsigned int *temp_buffer =
      (unsigned int *)malloc(total_pixels * sizeof(unsigned int));
  if (!temp_buffer) {
    return;
  }

  // Sauvegarde du buffer source
  for (int i = 0; i < total_pixels; i++) {
    temp_buffer[i] = buffer[i];
  }

  // Noyau 1D vertical : [ -1, 3, -1 ]
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int idx = y * width + x;

      // Ignorer les bords supérieur et inférieur
      if (y == 0 || y == height - 1) {
        continue;
      }

      unsigned int center = temp_buffer[idx];
      unsigned int top = temp_buffer[(y - 1) * width + x];
      unsigned int bottom = temp_buffer[(y + 1) * width + x];

      // Extraction RVB et Alpha
      int rc = (center >> 16) & 0xFF;
      int gc = (center >> 8) & 0xFF;
      int bc = center & 0xFF;
      unsigned int alpha = center & 0xFF000000;

      int rt = (top >> 16) & 0xFF;
      int gt = (top >> 8) & 0xFF;
      int bt = top & 0xFF;

      int rb = (bottom >> 16) & 0xFF;
      int gb = (bottom >> 8) & 0xFF;
      int bb = bottom & 0xFF;

      // Application du filtre vertical : 3 * C - Top - Bottom
      //int r_out = 3 * rc - rt - rb;
      //int g_out = 3 * gc - gt - gb;
      //int b_out = 3 * bc - bt - bb;
      int w=13;
      int r_out = rc + ((w * (2*rc - rt - rb) + 128) >> 8);
      int g_out = gc + ((w * (2*gc - gt - gb) + 128) >> 8);
      int b_out = bc + ((w * (2*bc - bt - bb) + 128) >> 8);

      // Saturation (Clamping)
      r_out = r_out < 0 ? 0 : (r_out > 255 ? 255 : r_out);
      g_out = g_out < 0 ? 0 : (g_out > 255 ? 255 : g_out);
      b_out = b_out < 0 ? 0 : (b_out > 255 ? 255 : b_out);

      buffer[idx] = alpha | ((unsigned int)r_out << 16) |
                    ((unsigned int)g_out << 8) | (unsigned int)b_out;
    }
  }

  free(temp_buffer);
}

void applyHorizontalSharpen( unsigned int *buffer, int width, int height) {
  if (width < 3 || height <= 0 || buffer == NULL) {
    return;
  }

  int total_pixels = width * height;
  unsigned int *temp_buffer =
      (unsigned int *)malloc(total_pixels * sizeof(unsigned int));
  if (!temp_buffer) {
    return;
  }

  // Sauvegarde du buffer source
  for (int i = 0; i < total_pixels; i++) {
    temp_buffer[i] = buffer[i];
  }

  // Noyau 1D horizontal : [ -1, 3, -1 ]
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int idx = y * width + x;

      // Ignorer les bords gauche et droit
      if (x == 0 || x == width - 1) {
        continue;
      }

      unsigned int center = temp_buffer[idx];
      unsigned int left = temp_buffer[y * width + (x - 1)];
      unsigned int right = temp_buffer[y * width + (x + 1)];

      // Extraction RVB et Alpha
      int rc = (center >> 16) & 0xFF;
      int gc = (center >> 8) & 0xFF;
      int bc = center & 0xFF;
      unsigned int alpha = center & 0xFF000000;

      int rl = (left >> 16) & 0xFF;
      int gl = (left >> 8) & 0xFF;
      int bl = left & 0xFF;

      int rr = (right >> 16) & 0xFF;
      int gr = (right >> 8) & 0xFF;
      int br = right & 0xFF;

      // Application du filtre horizontal : 3 * C - Left - Right
      //int r_out = 3 * rc - rl - rr;
      //int g_out = 3 * gc - gl - gr;
      //int b_out = 3 * bc - bl - br;
      int w=13;
      int r_out = rc + ((w * (2*rc - rl - rr) + 128) >> 8);
      int g_out = gc + ((w * (2*gc - gl - gr) + 128) >> 8);
      int b_out = bc + ((w * (2*bc - bl - br) + 128) >> 8);

      // Saturation (Clamping)
      r_out = r_out < 0 ? 0 : (r_out > 255 ? 255 : r_out);
      g_out = g_out < 0 ? 0 : (g_out > 255 ? 255 : g_out);
      b_out = b_out < 0 ? 0 : (b_out > 255 ? 255 : b_out);

      buffer[idx] = alpha | ((unsigned int)r_out << 16) |
                    ((unsigned int)g_out << 8) | (unsigned int)b_out;
    }
  }

  free(temp_buffer);
}

