#include "environment.hpp"

#include <cassert>

#include "util.hpp"

// Wall locations
const bool Pacman::maze[size][size] = {
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 0, 1},
    {1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1},
    {1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1},
    {1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1},
    {1, 1, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 1},
    {0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0},
    {1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1},
    {1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1},
    {1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1},
    {1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
    {1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1},
    {1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1},
    {1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}
    };


/* Utilities */
void Pacman::printWorld(void) {
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            switch (world[i][j]) {
                case e_empty    : std::cout << " "; break;
                case e_wall     : std::cout << "\u2588"; break;
                case e_food     : std::cout << "\u2022"; break;
                case e_pacman   : std::cout << "P"; break;
                case e_ghost    : std::cout << "G"; break;
                case e_gf       : std::cout << "G"; break;
                default         : std::cout << "x"; break;
            }
        }
        std::cout << std::endl;
    }
}

// Update entity positions only
void Pacman::updateWorldPositions(void) {
    world[pacman.row][pacman.col] = e_pacman;
    for (int i = 0; i < numGhosts; i++) {
        int y = ghosts[i].row;
        int x = ghosts[i].col;
        world[y][x] = world[y][x] == e_food ? (int) e_gf : e_ghost;
    }
}

// Check if a particular entity is at a particular position p
bool Pacman::entityAt(int row, int col, const int ent) {
    assert(row >= 0 && row < size);
    assert(col >= 0 && col < size);
    int actual = world[row][col];
    if (actual == ent) return true;
    else if (ent == e_ghost) return actual == e_gf;
    else if (ent == e_food) return actual == e_gf;
    return false;
}

/** Check if an entity is in line of sight
    @param p Current position
    @param dir Cardinal direction for sight
    @param ent The entity to check for
    @return boolean, True if the entity can be seen
*/
bool Pacman::lineOfSight(point &p, action_t dir, const int ent) {
    assert(dir < m_num_actions);
    switch (dir) {
        case m_move_left :
            for (int i = p.col; i >= 0; --i) {
                if (entityAt(p.row, i, ent)) return true;
            }
            break;
        case m_move_right :
            for (int i = p.col; i < size; ++i) {
                if (entityAt(p.row, i, ent)) return true;
            }
            break;
        case m_move_up :
            for (int i = p.row; i >= 0; --i) {
                if (entityAt(i, p.col, ent)) return true;
            }
            break;
        case m_move_down :
            for (int i = p.row; i < size; ++i) {
                if (entityAt(i, p.col, ent)) return true;
            }
            break;
        default : return 0;
    };
    return 0;
}

// Output a percept corresponding to pacman's current observation
percept_t Pacman::getObservation(void) {
    percept_t observation;
    // Hardcoded observation size
    bool bits[16];

    int curRow = pacman.row;
    int curCol = pacman.col;

    // 4 bits for wall configuration
    // Left
    bits[0] = (curCol - 1 < 0) ? 1 : maze[curRow][curCol - 1];
    // Right
    bits[1] = (curCol + 1 == size) ? 1 : maze[curRow][curCol + 1];
    // Up
    bits[2] = (curRow - 1 < 0) ? 1 : maze[curRow - 1][curCol];
    // Down
    bits[3] = (curRow + 1 == size) ? 1 : maze[curRow + 1][curCol];

    // 4 bits for ghost visibility (line of sight)
    bits[4] = lineOfSight(pacman, m_move_left, e_ghost);
    bits[5] = lineOfSight(pacman, m_move_right, e_ghost);
    bits[6] = lineOfSight(pacman, m_move_up, e_ghost);
    bits[7] = lineOfSight(pacman, m_move_down, e_ghost);

    // 3 bits for food "smell" (Manhattan Distance)
    // TODO
    bits[8] = 0;
    bits[9] = 0;
    bits[10] = 0;

    // 4 bits for food visibility (line of sight)
    bits[11] = lineOfSight(pacman, m_move_left, e_food);
    bits[12] = lineOfSight(pacman, m_move_right, e_food);
    bits[13] = lineOfSight(pacman, m_move_up, e_food);
    bits[14] = lineOfSight(pacman, m_move_down, e_food);

    // 1 bit for power pellet
    bits[15] = power;

}

/* Implementations required by Environment */
Pacman::Pacman(options_t &options) {
    // TODO: options
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            int entity = maze[i][j];
            // Place food in empty locations with 0.5 probability
            if (entity == 0) {
                entity = rand01() < 0.5 ? (int) e_food : 0;
            }
            world[i][j] = entity;
        }
    }
    // initial locations
    pacman.row = 12;
    pacman.col = 9;
    // Start without effects of power pellet
    power = 0;

    ghosts[0].row = 7;
    ghosts[0].col = 9;
    ghosts[1].row = 7;
    ghosts[1].col = 10;
    ghosts[2].row = 8;
    ghosts[2].col = 9;
    ghosts[3].row = 8;
    ghosts[3].col = 10;

    updateWorldPositions();

    m_observation = getObservation();
    m_reward = 0;
}

void Pacman::performAction(action_t action) {
    assert(action < 4);

}
