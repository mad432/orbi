#include "system.h"
#include <iostream>
#include <thread>
#define _USE_MATH_DEFINES
#include <math.h>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>
#include <CL/opencl.h>
#include <CL/cl.hpp>
#include <CL/cl.h>



std::mutex myMutex;

double step_ = 1;

double* System::step = &step_;

double g_ = .001;

double* System::g = &g_;

int size_ = 0;

int* System::size = &size_;

bool beencol_ = false;

bool* System::beencol = &beencol_;

float col_threshold_ = 50;

float* System::col_threshold = &col_threshold_;

int num_col_particles_ = 2;

int* System::num_col_particles = &num_col_particles_;

std::vector <Particle *> particles_;

std::vector <Particle *>* System::particles = &particles_;

std::vector<cl::Device> all_devices;

int store = cl::Platform::get().getDevices(CL_DEVICE_TYPE_ALL,&all_devices);

cl::Device System::default_device = all_devices[0];

cl::Context System::context({ default_device });

cl::CommandQueue System::queue(context, all_devices[0]);

cl::Program::Sources System::sources;
cl::Program System::program(context, sources);


cl::Buffer System::A_d(context, CL_MEM_READ_ONLY, sizeof(double) * 10000);
cl::Buffer System::B_d(context, CL_MEM_READ_ONLY, sizeof(double) * 10000);
cl::Buffer System::C_d(context, CL_MEM_READ_ONLY, sizeof(double) * 10000);
cl::Buffer System::D_d(context, CL_MEM_READ_ONLY, sizeof(double) * 10000);
cl::Buffer System::E_d(context, CL_MEM_READ_ONLY, sizeof(double) * 10000);
cl::Buffer System::F_d(context, CL_MEM_READ_ONLY, sizeof(double) * 10000);
cl::Buffer System::G_d(context, CL_MEM_READ_ONLY, sizeof(int) * 10000);
cl::Buffer System::H_d(context, CL_MEM_READ_ONLY, sizeof(double) * 10000);
cl::Buffer System::I_d(context, CL_MEM_WRITE_ONLY, sizeof(double) * 10000);
cl::Buffer System::J_d(context, CL_MEM_WRITE_ONLY, sizeof(double) * 10000);
sources.push_back({ kernel_code.c_str(),kernel_code.length() });



std::string kernel_code =
"        void kernel gravity(global const double* par1x, global const double* par1y, global const double* par2x, global const double* par2y, global const double* m1, global const double* m2, global const int* step, global const double* g, global double* forcex, global double* forcey){"

"                       double dist = sqrt(pow(par1x[get_global_id(0)] - par2x[get_global_id(0)] , 2) + pow(par1y[get_global_id(0)] - par2y[get_global_id(0)] , 2));"//calculates distance
""
"                       double grav = -(g[get_global_id(0)] * m1[get_global_id(0)] * m2[get_global_id(0)]) / pow(dist , 2);"//calculates gravitational force
""
"                       forcex[get_global_id(0)] = step[get_global_id(0)] * (g[get_global_id(0)] / 0.0048)  * grav * (par1x[get_global_id(0)] - par2x[get_global_id(0)]) / dist;"// calculates x component of acceleration due to gravity
"                       forcey[get_global_id(0)] = step[get_global_id(0)] * (g[get_global_id(0)] / 0.0048)  * grav * (par1y[get_global_id(0)] - par2y[get_global_id(0)]) / dist;"// y component of gravity
"          }";

System::System()
{


}
void System::Idreset(){

    //resets the particles index in the system

    std::vector <Particle *> hold = *particles;

    for(int id = 0; id < *size ; id++){
        hold[id]->setid(id);
    }

//    *particles = hold;
}

void System::Remove(int id, std::vector <Particle *> input){

    // removes a particle from index id

    const std::vector <Particle *> returned = *particles;

    particles->clear();

    for(int i = 0; i < *size; i++ ){
        if(i != id){
            particles->push_back(returned[i]);
        }
    }

    *size = particles->size();

    Idreset();

}


Particle* System::addParticle(Particle* par){

    //adds a particle to system

    particles->push_back(par);

    *size = *size + 1;

    return par;
}

Particle* System::addParticle(int Mass, long double _x, long double _y , long double _vx, long double _vy, bool fixed){

    //adds a particle to system

    Particle* par = new Particle( Mass, _x, _y , _vx, _vy, fixed, *size);

    particles->push_back(par);

    *size = *size + 1;

    return par;
}

void System::collision(Particle* par, Particle* par1){
    // collision between particles
    //std::cout<<par->getid()<<std::endl;

    if(!par->getcol() && !par1->getcol()){
        par->hascol();
        par1->hascol();

        double posx = (par1->getx()*par1->Getmass()+par->getx()*par->Getmass())/(par->Getmass()+par1->Getmass());// new particle position

        double posy = (par1->gety()*par1->Getmass()+par->gety()*par->Getmass())/(par->Getmass()+par1->Getmass());


        double rel_vel_x = par->getvx() - par1->getvx();//relative velocities
        double rel_vel_y = par->getvy() - par1->getvx();

        double rel_speed = sqrt(pow(rel_vel_x,2)+pow(rel_vel_y,2));
        double relunitvecx = rel_vel_x/rel_speed;
        double relunitvecy = rel_vel_y/rel_speed;

        double rel_pos_x = par->getx() - par1->getx();//relative position
        double rel_pos_y = par->gety() - par1->gety();

        double rel_d_pos = sqrt(pow(rel_pos_x,2)+pow(rel_pos_y,2));
        double relunitposx = rel_pos_x/rel_d_pos;
        double relunitposy = rel_pos_y/rel_d_pos;

        double collision_angle = acos((relunitvecx*relunitposx)+(relunitvecy*relunitposy))/M_PI * 90;
        //std::cout<<"angle : "<<collision_angle<<std::endl;
        //std::cout<<"relunitposx : "<<relunitposy <<std::endl;
        double relmass = 0;
        double mass1 = par1->Getmass();
        double mass = par->Getmass();
        if(par->Getmass()>par1->Getmass()){
            relmass = mass1/(mass);
        }else {
            relmass = mass/(mass1);
        }
//        std::cout<<"relmass: "<<relmass<<std::endl;


//        std::cout<<"calc : "<</*(abs(par->Getmass() - par1->Getmass()))*/ 100000 / ( 1/rel_speed / ((collision_angle)+10)) / (0.5 + relmass * 4) <<std::endl;


        double breakmass = 0;
        double break_mom_x = 0;//break away particles momentum
        double break_mom_y = 0;
        if((1000) / ( 1/rel_speed * ((collision_angle+20))+1) * (0.1 + relmass * 0.5)  > *col_threshold && rel_speed > 20){//determines if any new particles will be created from collision
            //std::cout<<"new Particles"<<std::endl;
            double breaksize;
            double breakvelx;
            double breakvely;

            if(par->Getmass()>par1->Getmass()){//determines the smallest particle in the collision
                breaksize = (par1->Getmass()/5);//breakaway mass

            }else{
                breaksize = (par->Getmass()/5);

            }if (breaksize>2){
                bool side = true;//puts a particle on either side 
                for(int i = 0;i < *num_col_particles;i++){;
                    if(side){
                        breakvelx = (rel_speed*relunitposy*-1 - relunitposx*rel_speed) * .1;
                        breakvely = (rel_speed*relunitposx - relunitposy*rel_speed) * .1;
                        addParticle(breaksize,  par->getx()-(rel_pos_x)-(sqrt(breaksize/3.14))*relunitposx-2*relunitposy,  par->gety() - (rel_pos_y)-(sqrt(breaksize/3.14)) * relunitposy+2*relunitposx,  breakvelx,  breakvely, 0);
                    }else{
                        breakvelx = (rel_speed*relunitposy - relunitposx*rel_speed) * .1;
                        breakvely = (rel_speed*relunitposx*-1 - relunitposy*rel_speed) * .1;
                        addParticle(breaksize,  par->getx()-(rel_pos_x)-(sqrt(breaksize/3.14))*relunitposx+2*relunitposy,  par->gety() - (rel_pos_y)-(sqrt(breaksize/3.14)) * relunitposy-2*relunitposx,  breakvelx,  breakvely, 0);
                    }
                    break_mom_x += breakvelx * breaksize;
                    break_mom_y += breakvely * breaksize;
                    breakmass += breaksize;
                    side = !side;
                    //std::cout<<side<<std::endl;
                }
            }
        }

        double vx = (par1->getvx()*par1->Getmass()+par->getvx()*par->Getmass()-break_mom_x)/(par->Getmass()+par1->Getmass());//new particle velocity

        double vy = (par1->getvy()*par1->Getmass()+par->getvy()*par->Getmass()-break_mom_y)/(par->Getmass()+par1->Getmass());

        bool fix = false;

        if (par->getfix() || par1->getfix()){
            fix = true;
        }

        addParticle(par->Getmass()+par1->Getmass() - breakmass, posx , posy , vx , vy , fix);


        //Idreset();
    }

}
void System::clear(){

    particles->clear();
    *size = 0;

}

cords System::gravity1(double par1x , double par1y , double par2x , double par2y , double m1, double m2){
    //input x and y cordiantes and mass of 2 particles
    //returns gravitational attraction
    long double dist = sqrt(pow(par1x - par2x , 2) + pow(par1y - par2y , 2));

    long double grav = -(*g * m1 * m2) / pow(dist , 2);

    cords ret;

    ret.x = *step * (*g / 0.0048)  *grav * (par1x - par2x) / dist;// calculates x component of acceleation due to gravity
    ret.y = *step * (*g / 0.0048)  *grav * (par1y - par2y) / dist;// y component of gravity


//    std::cout<<ret.y<<":";
//    std::cout<<ret.x<<std::endl;
    return ret;

}




bool System::process(){
    //allocates threads to particles

    int parper = *size/threads + 1;

    if(*size == 0){return false;}

    const int length = *size;

    //std::cout<<parper<<std::endl;

    std::thread *Mythreads=new std::thread[threads];

    int start =  parper;

    *beencol = false;

    for(int i = 0; i < threads ; i++ ){
        //std::lock_guard<std::mutex> guard(myMutex);
        if(start < length - parper){

            Mythreads[i] = std::thread(update, start, start + parper);

            //std::cout<<"thread : "<<i<<" from :"<<start<<" to :"<<start+parper<<std::endl;

        }else{

            Mythreads[i] = std::thread(update, start, length);

            //std::cout<<"thread : "<<i<<" from :"<<start<<" to :"<<*size<<std::endl;

        }
        start = start + parper;
    }

    update(0,parper);

    for(int i = 1; i < threads ; i++ ){
        Mythreads[i].join();
    }

    if(*beencol){
        const std::vector <Particle *> hold = *particles;
        for(auto &par : *particles){
            //par->hascol();
            if(par->getcolnum() != -1 && !par->getcol()){
                for(auto &par1 : *particles){
                    if(par->getcolnum()==par1->getid()){
                        collision(par,par1);
                    }
                }
            }
        //std::cout<<par->getcolnum()<<std::endl;
        }
    }
//    tickcount++;
//    std::cout<<tickcount<<std::endl;

    return *beencol;

};
bool System::update(int start, int end){
    //takes the first par id to compute to the last particle there respective motion
    //returns if a colision has occured

    bool col = false; //if collision

    const std::vector <Particle *> hold = *particles;

    double stepfactor = .0005;

    for (int i = start ; i < end ; i++){

        Particle* par = hold[i];

        if(par->getfix() == false){

            par->setx(par->getx() + par->getvx() **step * stepfactor);//move particle

            par->sety(par->gety() + par->getvy() **step * stepfactor);


           int j = 0;
           double* parx=new double[*size ];
           double* pary=new double[*size ];
           double* parvx=new double[*size ];
           double* parvy=new double[*size ];
           double* parm=new double[*size ];
            for(auto &par1 : hold){
                if(par1->getid() != par->getid() && par1->getcolnum() == -1){

                    if((par->getsize() + par1->getsize()) / 2 > sqrt(pow(par->getx() - par1->getx(),2) + pow(par->gety() - par1->gety() , 2))){//check for collision

                        if(!par->getcol() && !par1->getcol()){//check to see if a particle has already collided

                            par->setcol(par1->getid());

                            par1->setcol(par->getid());

                        }
                        *beencol = true;


                    }else{
                       parx[j] = par1->getx();
                       pary[j] = par1->gety();
                       parvy[j] = par1->getvy();
                       parvx[j] = par1->getvx();
                       parm[j] = par1->Getmass();

                    }                    
                    j++;
                }
            }

           double * cparx=new double[*size];//  OPENCL implementation currently kindof working though 1)slower 2) prone to crashing
           double * cpary=new double[*size];
           double * cparm=new double[*size];
           int * cstep=new int[*size];
           double * cg = new double[*size];
           for(int j = 0;j<*size;j++){
               cparx[j]=par->getx();
               cpary[j]=par->gety();
               cparm[j]=par->Getmass();
               cstep[j]=*step;
               cg[j]=*g;
           }
           myMutex.lock();

           //std::cout<<"B_h:"<<B_h[0]<<std::endl;


           queue.enqueueWriteBuffer(A_d, CL_TRUE, 0, sizeof(double) * *size, cparx);
           queue.enqueueWriteBuffer(B_d, CL_TRUE, 0, sizeof(double) * *size, cpary);
           queue.enqueueWriteBuffer(C_d, CL_TRUE, 0, sizeof(double) * *size, parx);
           queue.enqueueWriteBuffer(D_d, CL_TRUE, 0, sizeof(double) * *size, pary);
           queue.enqueueWriteBuffer(E_d, CL_TRUE, 0, sizeof(double) * *size, parm);
           queue.enqueueWriteBuffer(F_d, CL_TRUE, 0, sizeof(double) * *size, cparm);
           queue.enqueueWriteBuffer(G_d, CL_TRUE, 0, sizeof(int) * *size, cstep);
           queue.enqueueWriteBuffer(H_d, CL_TRUE, 0, sizeof(double) * *size, cg);

           if (program.build({ default_device }) != CL_SUCCESS) {
               std::cout << " Error building: " << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(default_device) << "\n";
               exit(1);
           }

           cl::make_kernel<cl::Buffer, cl::Buffer, cl::Buffer, cl::Buffer, cl::Buffer, cl::Buffer, cl::Buffer, cl::Buffer, cl::Buffer, cl::Buffer> gravity(cl::Kernel(program, "gravity"));

           cl::NDRange global(*size);
           gravity(cl::EnqueueArgs(queue, global), A_d, B_d, C_d, D_d, E_d, F_d , G_d, H_d, I_d, J_d).wait();

           double* I_h = new double[*size];
           queue.enqueueReadBuffer(I_d, CL_TRUE, 0, sizeof(double) * *size, I_h);
           double* J_h = new double[*size];
           queue.enqueueReadBuffer(J_d, CL_TRUE, 0, sizeof(double) * *size, J_h);
           myMutex.unlock();

           for (int i = 0; i<*size-1; i++) {
               par->setvx(par->getvx() + (I_h[i] / par->Getmass()) * 2);
               par->setvy(par->getvy() + (J_h[i] / par->Getmass()) * 2);
           }
          //std::cout<<std::endl;


           delete [] cg;
           delete [] cstep;
           delete [] cparx;
           delete [] cpary;
           delete [] cparm;

           delete[] parx;
           delete[] pary;
           delete[] parvx;
           delete[] parvy;
           delete[] parm;





        }


    }

    return col;
}
