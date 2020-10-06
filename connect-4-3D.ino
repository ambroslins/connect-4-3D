#include "FastLED.h"

#define SIZE_X 4
#define SIZE_Y 4
#define SIZE_Z 4

#define CHIPSET WS2812B

#define add_led_column(x, y, input_pin, led_pin)              \
  FastLED.addLeds<CHIPSET, led_pin, GRB>(leds[x][y], SIZE_Z); \
  pinMode(input_pin, INPUT_PULLUP);                           \
  input_pins[x][y] = input_pin;

CRGB leds[SIZE_X][SIZE_Y][SIZE_Z];
CRGB underfloor[84];

uint8_t input_pins[SIZE_X][SIZE_Y];

struct Input {
  enum class State { Ready, Debouncing, Pressed };
  State state = State::Ready;

  uint8_t x, y;

  const unsigned int debounce_time = 50;
  unsigned int time = 0;

  void update(unsigned int dt) {
    switch (state) {
      case State::Ready:
        for (size_t i = 0; i < SIZE_X; i++) {
          for (size_t j = 0; j < SIZE_Y; j++) {
            if (digitalRead(input_pins[i][j]) == LOW) {
              x = i;
              y = j;
              time = 0;
              state = State::Debouncing;
              return;
            }
          }
        }
        break;
      case State::Debouncing:
        if (digitalRead(input_pins[x][y]) == LOW) {
          time += dt;
          if (time >= debounce_time) {
            time = 0;
            state = State::Pressed;
          }
        } else {
          time = 0;
          state = State::Ready;
        }
        break;
      case State::Pressed:
        if (digitalRead(input_pins[x][y]) == HIGH) {
          if (time >= debounce_time) {
            state = State::Ready;
          } else {
            time += dt;
          }
        } else {
          time = 0;
        }
        break;

      default:
        break;
    }
  }
};

Input input;

struct Game {
  enum class State { Ready, Dropping, Draw, Winner, Reset };
  State state = State::Ready;

  enum class Piece { None, Yellow, Red };
  Piece grid[SIZE_X][SIZE_Y][SIZE_Z] = {};
  Piece* current_column = grid[0][0];

  size_t winning_positions[3][4] = {};

  enum class Player { Yellow, Red };
  Player current_player = Player::Yellow;

  static Player next_player(Player p) {
    switch (p) {
      case Player::Yellow:
        return Player::Red;
      case Player::Red:
        return Player::Yellow;
    }
  }

  static Piece player_to_piece(Player p) {
    switch (p) {
      case Player::Yellow:
        return Piece::Yellow;
      case Player::Red:
        return Piece::Red;
    }
  }

  unsigned int time = 0;
  const unsigned int drop_time = 500;

  static CRGB piece_to_crgb(Piece p) {
    switch (p) {
      case Piece::None:
        return CRGB::Black;
      case Piece::Yellow:
        return CRGB::Yellow;
      case Piece::Red:
        return CRGB::Red;
    }
  }

  bool full() {
    for (size_t i = 0; i < SIZE_X; i++) {
      for (size_t j = 0; j < SIZE_Y; j++) {
        for (size_t k = 0; k < SIZE_Z; k++) {
          if (grid[i][j][k] == Piece::None) {
            return false;
          }
        }
      }
    }
    return true;
  }

  bool won() {
    const Piece current = player_to_piece(current_player);
    const auto check = [this, current](size_t x, size_t y, size_t z) {
      const auto p = [this, current, x, y, z](int i, int j, int k) {
        for (size_t n = 0; n < 4; n++) {
          const int tx = x + i * n;
          const int ty = y + j * n;
          const int tz = z + k * n;

          if (tx >= 4 || ty >= 4 || tz >= 4 || tz < 0 ||
              grid[tx][ty][tz] != current) {
            return false;
          } else {
            winning_positions[n][0] = tx;
            winning_positions[n][1] = ty;
            winning_positions[n][2] = tz;
          }
        }
        return true;
      };
      for (size_t i = 0; i <= 1; i++) {
        for (size_t j = 0; j <= 1; j++) {
          for (int k = -1; k <= 1; k++) {
            if (p(i, j, k)) {
              return true;
            }
          }
        }
      }
    };
    for (size_t i = 0; i < 4; i++) {
      for (size_t j = 0; j < 4; j++) {
        if (check(i, j, 0) || check(i, 0, j) || check(0, i, j)) {
          return true;
        }
      }
    }
  }

  void update(unsigned int dt) {
    switch (state) {
      case State::Ready:
        if (input.state == Input::State::Pressed) {
          current_column = grid[input.x][input.y];
          Piece& top_piece = current_column[SIZE_Z - 1];
          if (top_piece == Piece::None) {
            top_piece = player_to_piece(current_player);
            state = State::Dropping;
          }
        }
        break;
      case State::Dropping:
        time += dt;
        if (time < drop_time) {
          return;
        }
        time = 0;
        for (size_t i = 1; i < SIZE_Z; i++) {
          if (current_column[i] != Piece::None &&
              current_column[i - 1] == Piece::None) {
            current_column[i - 1] = current_column[i];
            current_column[i] = Piece::None;
          }
        }
        for (size_t i = 1; i < SIZE_Z; i++) {
          if (current_column[i] != Piece::None &&
              current_column[i - 1] == Piece::None) {
            return;
          }
        }
        if (input.state == Input::State::Ready) {
          if (won()) {
            state = State::Winner;
          } else if (full()) {
            state = State::Draw;
          } else {
            current_player = next_player(current_player);
            state = State::Ready;
          }
        }
        break;
      case State::Draw:
      case State::Winner:
        if (input.state == Input::State::Pressed) {
          for (size_t i = 0; i < SIZE_X; i++) {
            for (size_t j = 0; j < SIZE_Y; j++) {
              for (size_t k = 0; k < SIZE_Z; k++) {
                grid[i][j][k] = Piece::None;
              }
            }
          }
          current_player = next_player(current_player);
          state = State::Reset;
        }
        break;
      case State::Reset:
        if (input.state == Input::State::Ready) {
          state = State::Ready;
        }
        break;
      default:
        break;
    }
  }
  void show(unsigned int dt) {
    const uint8_t d = min(dt, 255);
    const bool blink = (state == State::Winner) && (millis() / 500) | 1;

    for (size_t i = 0; i < SIZE_X; i++) {
      for (size_t j = 0; j < SIZE_Y; j++) {
        for (size_t k = 0; k < SIZE_Z; k++) {
          CRGB& led = leds[i][j][k];
          const CRGB new_rgb = piece_to_crgb(grid[i][j][k]);
          const CRGB old_rgb = led;
          if (old_rgb != new_rgb) {
            const int dr = new_rgb.r - old_rgb.r;
            const int dg = new_rgb.g - old_rgb.g;
            const int db = new_rgb.b - old_rgb.b;
            led.r += constrain(dr, -d, d);
            led.g += constrain(dg, -d, d);
            led.b += constrain(db, -d, d);
          }
        }
      }
    }
    for (auto& l : underfloor) {
      l = blink ? CRGB::Black : piece_to_crgb(player_to_piece(current_player));
    }

    if (blink) {
      for (auto p : winning_positions) {
        leds[p[0]][p[1]][p[2]] = CRGB::Black;
      }
    }

    FastLED.show();
  }
};

Game game;

unsigned long last_update;

void setup() {
  FastLED.addLeds<CHIPSET, 6, GRB>(underfloor, 84);
  add_led_column(0, 0, 22, 23);
  add_led_column(0, 1, 24, 25);
  add_led_column(0, 2, 26, 27);
  add_led_column(0, 3, 28, 29);
  add_led_column(1, 0, 30, 31);
  add_led_column(1, 1, 32, 33);
  add_led_column(1, 2, 34, 35);
  add_led_column(1, 3, 36, 37);
  add_led_column(2, 0, 38, 39);
  add_led_column(2, 1, 40, 41);
  add_led_column(2, 2, 42, 43);
  add_led_column(2, 3, 44, 45);
  add_led_column(3, 0, 46, 47);
  add_led_column(3, 1, 48, 49);
  add_led_column(3, 2, 50, 51);
  add_led_column(3, 3, 52, 53);
  last_update = millis();
}

void loop() {
  const unsigned long time = millis();
  const unsigned int dt = time - last_update;
  input.update(dt);
  game.update(dt);
  game.show(dt);
  last_update = time;
}
