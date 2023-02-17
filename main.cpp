#include "FEHLCD.h"
#include "FEHUtility.h"
#include "iostream"
#include "fstream"
#include "math.h"
#include "vector"
#include "functional"
#include "algorithm"

#define WIDTH 320  // px
#define HEIGHT 240 // px

#define DIFFICULTY 0.5 //asteroids added per second
#define TIME_VALUE 10 //points passively gained per second of survival

#define SCORES_FILE "Scores.dat" //file to read and save scores to

//Ship properties
#define SHIP_MAX_SPEED 220 // px/s
#define SHIP_ACCELERATION 400 // px/s^2
#define SHIP_DECELERATION 1.5 // px/s^2
#define SHIP_STOP_SPEED 4 // px/s
#define SHIP_SIZE 2 // arbitrary scalar
#define SHIP_WIDTH_FACTOR 0.7 // arbitrary scalar
#define SHIP_COLLISION_SIZE 5 // px radius
#define SHIP_FIRERATE 0.12 // Seconds between shots (lower is faster)
#define SHIP_SPAWN_EXCUSION_RADIUS 100 //Asteroid destory radius on spawn

//Asteroid properties
#define AST_COUNT 15 //2 integer number of starting asteroids 
#define AST_SPEED 45 // px/s
#define AST_SHIP_EXCUSION_RADIUS 50 //px radius around the ship where asteroids won't spawn
#define AST_SIZES 3 //Number of asteroid sizes (MUST BE GREATER THAN 2) If you want one size just set smallest and biggest equal
#define AST_SMALLEST 7 // px radius of smallest asteroids 
#define AST_BIGGEST 16 // px radius of biggest asteroids
#define AST_VALUE 100
const float AST_SIZE_FACTOR = (AST_BIGGEST-AST_SMALLEST)/(AST_SIZES-1.0); //to make later calculations faster

//Laser properties
#define LASER_SPEED 600 // px/s //Must be greater than ship_max_speed
#define LASER_LENGTH 7 // px
#define LASER_COLLISION_SIZE 1 // px radius
#define LASER_COLOR tigrRGB(60,255,20)
#define LASER_PIERCE false // bool laser is destroyed on contact
#define LASER_PENALTY 3 //float Points lost per laser fired

#include "tigr.h"
//Get access to the Tigr bitmap object used in the FEHLCD library
extern Tigr *screen;

//Function prototypes
void DrawShip(float x, float y, float hdg);
void SpawnAsteroid(float x = -1.0, float y = -1.0, float size = -1.0);
void SpawnShip(void);

//Scoreboard
class Scoreboard {
    public:
        float score;
        char cscore[20], chighscore[20];
        int old_score, highscore;
        std::vector<int> old_scores;
        
    Scoreboard(void){
        score = 0;
    }
    void add_size(float size){
        score += AST_VALUE/size;
    }
    void add_time(float dt){
        score += TIME_VALUE*dt;
    }
    void remove_shoot(void){
        score -= LASER_PENALTY;
    }
    float get_score(void){
        return score;
    }
    void display(void){
        sprintf(cscore, "Score: %09d", int(score)*100); 
        sprintf(chighscore, "Highscore: %09d", highscore);
        if(int(score)*100 > highscore) LCD.SetFontColor(GOLD);
        LCD.WriteAt(cscore, WIDTH-194, 21);
        if(int(score)*100 > highscore) LCD.SetFontColor(WHITE);
        LCD.WriteAt(chighscore, WIDTH-242, 1);
    }
    void reset(void){
        score = 0;
    }
    void load(const char file_path[99]){
        std::ifstream highscores_in (file_path); //Open for reading
        while(highscores_in>>old_score){
            old_scores.push_back(old_score);
        }
        highscores_in.close();
        if(old_scores.size()>0){
            highscore = *std::max_element(old_scores.begin(), old_scores.end());
            std::cout << highscore << std::endl;
        }
    }
    void save(const char file_path[99]){
        if(score>0){
            std::ofstream highscores_out (file_path, std::ios::app); //Open file for appending
            highscores_out << int(score)*100 << std::endl;
            highscores_out.close();
        }
    }
};

//Entity base class (pretty much anything that moves)
class Entity {
    public:
        //Position
        float x, y;
        //Speed
        float dx, dy;
        //Collision/draw size
        float size;

        //Entity Constructor
        Entity(float start_x, float start_y, float start_dx, float start_dy, float start_size){
            x = start_x;
            y = start_y;
            dx = start_dx;
            dy = start_dy;
            size = start_size;
        }

        virtual void explode(void){}
        virtual int update(float dt){
            //Velocity and make sure the entity is on screen
            x = fmod(x + WIDTH  + dx*dt, WIDTH );
            y = fmod(y + HEIGHT + dy*dt, HEIGHT);

            return 0;
        }

        virtual ~Entity(){};
};

//todo alright i need a header
//Vector of entities on screen (starting with the ship)
//?It's global because I'm a bad person
std::vector<Entity*> Entities;
std::vector<Entity*> Delete_queue;
std::vector<Entity*> Add_queue;

//Create scoreboard
Scoreboard scoreboard;

class Asteroid : public Entity {
public:
    Asteroid(float start_x, float start_y, float start_dx, float start_dy, float start_size) : Entity(start_x, start_y, start_dx, start_dy, start_size * AST_SIZE_FACTOR + AST_SMALLEST){
    }
    void explode(void){
        Entity::explode();

        if(size > AST_SMALLEST){ //If the asteroid isn't the smallest size, split it
            LCD.SetFontColor(ORANGERED);
            LCD.FillCircle(x,y,size);
            SpawnAsteroid((size-AST_SMALLEST)/AST_SIZE_FACTOR-1,x,y);
            SpawnAsteroid((size-AST_SMALLEST)/AST_SIZE_FACTOR-1,x,y);
            
        } else {
            LCD.SetFontColor(GREEN); //If it IS the smallest size, destroy it
            LCD.FillCircle(x,y,size);
        }
        //Update score
        scoreboard.add_size(size);
        LCD.SetFontColor(WHITE);
    }

    int update(float dt){
        Entity::update(dt);
        LCD.SetFontColor(BLACK);
        LCD.FillCircle(x,y,size-1);
        LCD.SetFontColor(WHITE);
        LCD.DrawCircle(x,y,size);
        return 0;
    }
};

class Laser : public Entity {
    public:
        float angle, tail_x, tail_y, delta_x, delta_y;
        Laser(float start_x, float start_y, float ang) : Entity(WIDTH/2,HEIGHT/2,0,0,LASER_COLLISION_SIZE) {
            tail_x= start_x;
            tail_y= start_y;
            angle = ang;
            dx = sin(angle);
            dy = cos(angle);
            x = tail_x - dx*LASER_LENGTH;
            y = tail_y - dy*LASER_LENGTH;
            dx *= -LASER_SPEED; // Some shenanigans goes on here to avoid calculating sin or cos twice per loop
            dy *= -LASER_SPEED;
        }

        int update(float dt){
            //Calculations are done after drawing for more reponsive hitreg.
            tigrLine(screen,tail_x,tail_y,x,y,LASER_COLOR);
            //Don't call for this one. It's too different Entity::update(dt);

            //Update positions
            delta_x = dx*dt; 
            delta_y = dy*dt; 
            tail_x += delta_x;
            tail_y += delta_y;
            x += delta_x;
            y += delta_y;

            if ((tail_x>=WIDTH) || (tail_x<=0) || (tail_y>=HEIGHT) || (tail_y<=0)){
                return 1;
            } else {
                return 0;
            }
        }
};

class Ship : public Entity {
    public:
        float d2x, d2y; //Acceleration
        float norm_factor; //Normalization factor (for calculations)
        float heading; //Heading (in radians)
        bool right, left, up, down; //Keyboard input 
        int mouse_x, mouse_y, mouse_buttons; //Mouse input
        float cooldown; //Laser cooldown timer

        //Spawn the ship in the middle of the screen
        Ship() : Entity(WIDTH/2, HEIGHT/2, 0, -0.01, SHIP_COLLISION_SIZE) {
            d2x = 0;
            d2y = 0;
            heading = 0;
        }

        int update(float dt){

            //Update velocities
            dx += d2x * dt;
            dy += d2y * dt;

            //Get user keyboard input for the ship
            Ship::control();

            //Limit to max speed (velocity vector normalization)
            if(pow(dx,2)+pow(dy,2)>=pow(SHIP_MAX_SPEED,2)){
                norm_factor = SHIP_MAX_SPEED/sqrt(pow(dx,2)+pow(dy,2));
                dx = dx*norm_factor;
                dy = dy*norm_factor;
            }

            //Calculate heading
            heading = atan2(dx,dy);

            //! SHIP PROPERTIES DISPLAY
            // LCD.WriteAt(dx,0,0);
            // LCD.WriteAt(dy,0,20);
            // LCD.WriteAt(d2x,120,0);
            // LCD.WriteAt(d2y,120,20);

            //Update position
            Entity::update(dt);
            if (dx>SHIP_STOP_SPEED || dx<-SHIP_STOP_SPEED) x += dx*dt;
            if (dy>SHIP_STOP_SPEED || dy<-SHIP_STOP_SPEED) y += dy*dt;
            
            //Draw the ship at current heading
            DrawShip(x,y,heading);

            shoot(dt);

            return 0;
        }

        void control(void){
            //Get user input for direction (exclusive)
            right = (tigrKeyHeld(screen, TK_RIGHT) || tigrKeyHeld(screen, 'D')) && !(tigrKeyHeld(screen, TK_LEFT ) || tigrKeyHeld(screen, 'A'));
            left  = (tigrKeyHeld(screen, TK_LEFT ) || tigrKeyHeld(screen, 'A')) && !(tigrKeyHeld(screen, TK_RIGHT) || tigrKeyHeld(screen, 'D'));
            up    = (tigrKeyHeld(screen, TK_UP   ) || tigrKeyHeld(screen, 'W')) && !(tigrKeyHeld(screen, TK_DOWN ) || tigrKeyHeld(screen, 'S'));
            down  = (tigrKeyHeld(screen, TK_DOWN ) || tigrKeyHeld(screen, 'S')) && !(tigrKeyHeld(screen, TK_UP   ) || tigrKeyHeld(screen, 'W'));

            //Since the inputs are exclusive, only 8 combinations are possible, so if else can be used
                 if (!right && !left && !up && !down){ d2x = -dx*SHIP_DECELERATION; d2y = -dy*SHIP_DECELERATION; } //No acceleration 
            else if ( right && !left && !up && !down){ d2x =  SHIP_ACCELERATION;    d2y = -dy*SHIP_DECELERATION; } //Accel right 
            else if (!right &&  left && !up && !down){ d2x = -SHIP_ACCELERATION;    d2y = -dy*SHIP_DECELERATION; } //Accel left
            else if (!right && !left &&  up && !down){ d2x = -dx*SHIP_DECELERATION; d2y = -SHIP_ACCELERATION;    } //Accel up
            else if (!right && !left && !up &&  down){ d2x = -dx*SHIP_DECELERATION; d2y =  SHIP_ACCELERATION;    } //Accel down
            else if ( right && !left &&  up && !down){ d2x =  M_SQRT1_2*SHIP_ACCELERATION; d2y = -M_SQRT1_2*SHIP_ACCELERATION; } //Accel ne
            else if ( right && !left && !up &&  down){ d2x =  M_SQRT1_2*SHIP_ACCELERATION; d2y =  M_SQRT1_2*SHIP_ACCELERATION; } //Accel se
            else if (!right &&  left && !up &&  down){ d2x = -M_SQRT1_2*SHIP_ACCELERATION; d2y =  M_SQRT1_2*SHIP_ACCELERATION; } //Accel sw
            else if (!right &&  left &&  up && !down){ d2x = -M_SQRT1_2*SHIP_ACCELERATION; d2y = -M_SQRT1_2*SHIP_ACCELERATION; } //Accel nw
            else{ d2x = -dx*SHIP_DECELERATION; d2y = -dy*SHIP_DECELERATION; } //Else, no acceleration (this should never be activated)
        }

        void shoot(float delta_time){
            //Shooting logic
            
            tigrMouse(screen, &mouse_x, &mouse_y, &mouse_buttons);
            if (mouse_buttons==0x01 || tigrKeyHeld(screen, TK_SPACE)){ //if the mouse is pressed
                if(cooldown>=SHIP_FIRERATE){ //and the cooldown is up
                    Add_queue.push_back(new Laser(x, y, atan2(x-mouse_x,y-mouse_y))); //spawn a laser
                    scoreboard.remove_shoot(); //Apply laser penalty
                    cooldown = 0;
                }
            }
            cooldown += delta_time;
        }
}; 


int main() {
//Declare variables
float frametime;
int current_size = 0, i , j, k, current_asteroids, start_time, round_time, difficulty_asteroids;
bool spawn_asteroids = true;

//Initalize tigrTime
frametime = tigrTime();

//Reset timer
start_time = TimeNow();

//?Create ship
Entities.push_back(new Ship());

//Load past scores
scoreboard.load(SCORES_FILE);

//Update cycle until the user presses esc
while (!tigrClosed(screen) && !tigrKeyDown(screen, TK_ESCAPE))
	{
        //Clear the screen
        LCD.Clear();
        frametime = tigrTime(); //Get time since tigrTime was last called

        //Count asteroids
        current_asteroids = 0;
        for(auto s: Entities){
            if ((typeid(Asteroid).name() == typeid(*s).name())){
                current_asteroids++;
            }
        }

        //Spawn asteroids up to the asteroid amount
        for(k = 0; k < AST_COUNT-current_asteroids+difficulty_asteroids; k++){
            if(Entities.size()>0) SpawnAsteroid(); //Check that something else exists first (wait one loop past respawn)
        }

        //Execute add queue
        for(Entity* a : Add_queue){
            Entities.push_back(a); //add to Entities
        }
        Add_queue.clear(); //clear the queue

        //Remove duplicates from the vector (really hard to do without sorting for some reason)
        std::sort(Delete_queue.begin(), Delete_queue.end());
        Delete_queue.erase(std::unique(Delete_queue.begin(), Delete_queue.end()), Delete_queue.end());

        //Remove lasers
        if(LASER_PIERCE){
            for(Entity* u : Delete_queue){
                if((typeid(Laser).name() == typeid(*u).name()))
                Delete_queue.erase(std::remove( Delete_queue.begin(),  Delete_queue.end(), u),  Delete_queue.end());
            }
        }
        //Free and remove every object in the delete queue from Entities //? might be able to do this in one shot idk
        for(Entity* d : Delete_queue){
            Entities.erase(std::remove(Entities.begin(), Entities.end(), d), Entities.end());
            delete d; //free memory
        }
        Delete_queue.clear(); //clear the queue

        //TODO Actual stuff goes here***************

        //Iterate through every entity and update it
        for(Entity* ent : Entities){
            if((ent->update(frametime))){ //Update every entity
                Delete_queue.push_back(ent); //If the object update returns an error, delete it (only for lasers)
            }
        }

        //Collision logic
        current_size = Entities.size();
        // Check for collisions
        for(i = 0; i < current_size-1; i++){
            for(j = i+1; j < current_size; j++){
                //Check if entities at i and j are collided
                if((typeid(Asteroid).name() != typeid(*Entities.at(i)).name()) || (typeid(Asteroid).name() != typeid(*Entities.at(j)).name()))
                if(pow(Entities.at(i)->size+Entities.at(j)->size,2) > pow(Entities.at(i)->x-Entities.at(j)->x,2) + pow(Entities.at(i)->y-Entities.at(j)->y,2)){
                    Entities.at(i)->explode();
                    Entities.at(j)->explode();
                    Delete_queue.push_back(Entities.at(i));
                    Delete_queue.push_back(Entities.at(j));
                }
            }
        }

        //Respawn ship
        if (tigrKeyDown(screen, 'R')){
            scoreboard.save(SCORES_FILE);
            scoreboard.load(SCORES_FILE);
            scoreboard.reset();
            Entities.clear();
            SpawnShip();
            start_time = TimeNow();
        }



        //round updates
        for(auto g: Entities){ //check if there is a ship for gaining time points          
            if(typeid(Ship).name() == typeid(*g).name()){
                if (spawn_asteroids) scoreboard.add_time(frametime); //don't add if theres no obstacles
                round_time = TimeNow() - start_time; //Time since the start of the round
                break;
            }
        }

        //? Dev control: Delete all asteroids
        if(tigrKeyDown(screen, 'O')){
            spawn_asteroids = false;
            for(Entity* o : Entities){
                if((typeid(Asteroid).name() == typeid(*o).name()))
                Delete_queue.push_back(o);
                std::cout << "deleting\n" ;
            }
        }

        //? Dev control: Add them back
        if(tigrKeyDown(screen, 'P')){
            spawn_asteroids = true;
        }

        if (tigrKeyDown(screen, 'I')){
            SpawnAsteroid();
        }

        if (tigrKeyDown(screen, 'Y')){
            SpawnShip();
        }

        //Number of asteroids on screen increases
        if(spawn_asteroids){
        difficulty_asteroids = round_time*DIFFICULTY;
        } else {
        difficulty_asteroids = -AST_COUNT;
        }
        scoreboard.display();
        //TODO ********************************
        //Show instantaneous FPS
        LCD.WriteAt(1/frametime,-2,HEIGHT-16);
        //Show number of asteroids
        LCD.WriteAt(AST_COUNT+difficulty_asteroids,WIDTH-40,HEIGHT-16);
        //Update screen
        LCD.Update();
    }

    //Save the current score
    scoreboard.save(SCORES_FILE);

    //Close the window
    tigrFree(screen);

return 0;
}

//Draw a triangle at a certain heading
void DrawShip(float x, float y, float hdg){

    //Points of the ship
    float p1x, p2x, p3x, p4x, p1y, p2y, p3y, p4y;
    p1x = x + SHIP_SIZE*3*cos(hdg - M_PI/2);
    p1y = y - SHIP_SIZE*3*sin(hdg - M_PI/2);

    p2x = x + SHIP_SIZE*3*cos(hdg - 3*M_PI/2 - SHIP_WIDTH_FACTOR);
    p2y = y - SHIP_SIZE*3*sin(hdg - 3*M_PI/2 - SHIP_WIDTH_FACTOR);
    
    p3x = x + SHIP_SIZE*3*cos(hdg - 3*M_PI/2 + SHIP_WIDTH_FACTOR);
    p3y = y - SHIP_SIZE*3*sin(hdg - 3*M_PI/2 + SHIP_WIDTH_FACTOR);

    p4x = x + SHIP_SIZE*1*cos(hdg - 3*M_PI/2);
    p4y = y - SHIP_SIZE*1*sin(hdg - 3*M_PI/2);

    //Draw triangle lines

    tigrLine(screen,p1x,p1y,p2x,p2y,tigrRGB(255,255,255));
    tigrLine(screen,p2x,p2y,p4x,p4y,tigrRGB(255,255,255));
    tigrLine(screen,p4x,p4y,p3x,p3y,tigrRGB(255,255,255));
    tigrLine(screen,p3x,p3y,p1x,p1y,tigrRGB(255,255,255));
};

void SpawnAsteroid(float size, float x, float y){
    //Parameters for the new asteroid object
    float ast_x, ast_y, ast_dx, ast_dy, ast_heading;
    float ast_size;
    bool too_close = true, ship_found=false;
    int tries = 0;

    //-1 values means spawn the asteroid in a random spot on the screen
    if ((x==-1.0)&&(y==-1.0)){

        while(too_close && tries<32){
            ast_x = fmod((rand()/10.0+WIDTH),WIDTH);
            ast_y = fmod((rand()/10.0+HEIGHT),HEIGHT);
            for(auto s: Entities){ //check if there is a ship too close             
                if(typeid(Ship).name() == typeid(*s).name()){ //check only ships
                    if (pow(AST_SHIP_EXCUSION_RADIUS, 2) < pow(s->x-ast_x,2)+pow(s->y-ast_y, 2)){
                        too_close=false;
                    }
                    ship_found = true;
                }
            }
            if(!ship_found){ //If there are no ships (respawn)
                if (pow(AST_SHIP_EXCUSION_RADIUS, 2) < pow(WIDTH/2-ast_x,2)+pow(HEIGHT/2-ast_y, 2)){
                    too_close=false;
                }
            }
            tries++;
        }

    } else { //Use the given arguments
        ast_x = x;
        ast_y = y;
    }

    //-1 values mean a random size
    if (size == -1.0){
        ast_size = rand()%AST_SIZES; // mod by the number of asteroid sizes
    } else {
        ast_size = size; //Use the given size
    }

    //Random direction used to calculate dx and dy
    ast_heading = rand();
    ast_dx = AST_SPEED * cos(ast_heading);
    ast_dy = AST_SPEED * sin(ast_heading);

    //Create a new asteroid object with those properties
    Add_queue.push_back(new Asteroid(ast_x, ast_y, ast_dx, ast_dy, ast_size));
}

void SpawnShip(void){
    for(auto r: Entities){ //check if there is a ship too close               
        if(typeid(Asteroid).name() == typeid(*r).name()){ //check only ships
            //LCD.DrawCircle(WIDTH/2, HEIGHT/2, SHIP_SPAWN_EXCUSION_RADIUS);
            if (pow(SHIP_SPAWN_EXCUSION_RADIUS, 2) > pow(WIDTH/2-r->x,2)+pow(HEIGHT/2-r->y, 2)){
                Delete_queue.insert(Delete_queue.begin(),r);
            }
        }
    }
    Add_queue.push_back(new Ship);
}