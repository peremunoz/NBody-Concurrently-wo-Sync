#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<math.h>
#include<time.h>
#include<stdio.h>
#include<unistd.h>
#include<pthread.h>

#ifdef D_GLFW_SUPPORT
    #include<GLFW/glfw3.h>
#endif

// Macros to make code a little bit easier to understand because for speedup reasons, I'll use only 1D arrays
#define PX(i) (3*i+1)
#define PY(i) (3*i+2)
#define MASS(i) (3*i+3)

#define VX(i) (4*i+0)
#define VY(i) (4*i+1)
#define AX(i) (4*i+2)
#define AY(i) (4*i+3)

double G=0.0001;
double dt=0.005;
double rcutoff=0.35;
double rlimit=0.03;

struct Node{
    struct Node *children[4];
    int external;

    double CMX;
    double CMY;
    double mass;
    double TRX;
    double TRY;

    double LLX;
    double LLY;

    double GCX;
    double GCY;
};

struct GraphicInterfaceStruct{
    struct Node* tree;
    double* sharedBuff;
    int* indexes;
    int nShared;
    int nLocal;
    double* localBuff;
    double* radius;
    int count;
    int steps;
    int nOriginal;
    int M;
};

struct BuildTreeStruct{
    struct Node* tree;
    double* sharedBuff;
    int* indexes;
    int nShared;
    int remainingThreads;
};

void buildTree(struct BuildTreeStruct* data){
    // Unpack the variables from the data struct
    struct Node* node = data->tree;
    double* shrdBuff = data->sharedBuff;
    int* indexes = data->indexes;
    int n = data->nShared;
    int remainingThreads = data->remainingThreads;

    // Create variables for controlling the concurrency
    int possibleSubThreads = 0;
    int remainingThreadsPerSubThread = 0;
    int extraThreadsPerSubThread = 0;

    pthread_t NE_Tid = 0, NW_Tid = 0, SW_Tid = 0, SE_Tid = 0;

    if(n==1){ //This is an external node!
        node->external=1;
        node->CMX=shrdBuff[PX(indexes[0])];
        node->CMY=shrdBuff[PY(indexes[0])];
        node->mass=shrdBuff[MASS(indexes[0])];
    } else {
        node->external=0;
		//Arrays of indexes of particles per quartile
        int *NEi = (int *) malloc(sizeof(int)*n);
        int *NWi = (int *) malloc(sizeof(int)*n);
        int *SWi = (int *) malloc(sizeof(int)*n);
        int *SEi = (int *) malloc(sizeof(int)*n);
        int NWc=0, SWc=0,SEc=0, NEc=0;

        int i;
		/** For each particle we will check where is it located relative to the geometric center,
			to sort them into the 4 children nodes**/
        for(i=0;i<n;i++){
            if(shrdBuff[PY(indexes[i])] < node->GCY ){ //South half
                if(shrdBuff[PX(indexes[i])] < node->GCX){ //West wing
                    SWi[SWc]=indexes[i];
                    SWc++;
                } else {
                    SEi[SEc]=indexes[i];
                    SEc++;
                }
            } else { //North half
                if(shrdBuff[PX(indexes[i])] < node->GCX){ //West wing
                    NWi[NWc]=indexes[i];
                    NWc++;
                } else {
                    NEi[NEc]=indexes[i];
                    NEc++;
                }
            }
        }

        // Pre-calculate how many possible subThreads we could create
        if(NEc>0) possibleSubThreads++;
        if(NWc>0) possibleSubThreads++;
        if(SWc>0) possibleSubThreads++;
        if(SEc>0) possibleSubThreads++;
        // How many threads can we assign to each subThread
        remainingThreadsPerSubThread = remainingThreads / possibleSubThreads;
        extraThreadsPerSubThread = remainingThreads % possibleSubThreads;


		//If there are particles in the NorthWest quarter
        if(NEc>0){
			//This instruction declares a new node in the position 0
            node->children[0]= malloc(sizeof *node->children[0]);
			//We give the values of the Low Left and Top Right corner, and also the geometric center.
            node->children[0]->TRX=node->TRX;
            node->children[0]->TRY=node->TRY;
            node->children[0]->LLX=node->GCX;
            node->children[0]->LLY=node->GCY;
            node->children[0]->GCX=(node->GCX+node->TRX)/2;
            node->children[0]->GCY=(node->GCY+node->TRY)/2;
			//We build a tree in the new node, with the particles that are inside
            // Create the build tree struct
            struct BuildTreeStruct* NE_Data = malloc(sizeof(struct BuildTreeStruct));
            NE_Data->tree = node->children[0];
            NE_Data->sharedBuff = shrdBuff;
            NE_Data->indexes = NEi;
            NE_Data->nShared = NEc;
            NE_Data->remainingThreads = 0;
            // If we have remaining threads to create, we build the sub-tree concurrently
            if (remainingThreads > 0) {
                int assignedThreadsToSubThread = remainingThreadsPerSubThread;

                if (extraThreadsPerSubThread > 0) {
                    assignedThreadsToSubThread++;
                    extraThreadsPerSubThread--;
                }

                remainingThreads = remainingThreads - assignedThreadsToSubThread;

                NE_Data->remainingThreads = assignedThreadsToSubThread;
                pthread_create(&NE_Tid, NULL, (void *(*) (void *)) buildTree, NE_Data);
            } else {
                buildTree(NE_Data);
            }
        } else {
			//If not, we set the children to null
            node->children[0]=NULL;
        }
		//The next three blocks are exactly the same thing but for the other three nodes
        if(NWc>0){
            node->children[1]= malloc(sizeof *node->children[1]);
            node->children[1]->TRX=node->GCX;
            node->children[1]->TRY=node->TRY;
            node->children[1]->LLX=node->LLX;
            node->children[1]->LLY=node->GCY;
            node->children[1]->GCX=(node->LLX+node->GCX)/2;
            node->children[1]->GCY=(node->GCY+node->TRY)/2;
            // Create the build tree struct
            struct BuildTreeStruct* NW_Data = malloc(sizeof(struct BuildTreeStruct));
            NW_Data->tree = node->children[1];
            NW_Data->sharedBuff = shrdBuff;
            NW_Data->indexes = NWi;
            NW_Data->nShared = NWc;
            NW_Data->remainingThreads = 0;
            // If we have remaining threads to create, we build the sub-tree concurrently
            if (remainingThreads > 0) {
                int assignedThreadsToSubThread = remainingThreadsPerSubThread;

                if (extraThreadsPerSubThread > 0) {
                    assignedThreadsToSubThread++;
                    extraThreadsPerSubThread--;
                }

                remainingThreads = remainingThreads - assignedThreadsToSubThread;

                NW_Data->remainingThreads = assignedThreadsToSubThread;
                pthread_create(&NW_Tid, NULL, (void *(*) (void *)) buildTree, NW_Data);
            } else {
                buildTree(NW_Data);
            }
        } else {
            node->children[1]=NULL;
        }
        if(SWc>0){
            node->children[2]= malloc(sizeof *node->children[2]);
            node->children[2]->TRX=node->GCX;
            node->children[2]->TRY=node->GCY;
            node->children[2]->LLX=node->LLX;
            node->children[2]->LLY=node->LLY;
            node->children[2]->GCX=(node->LLX+node->GCX)/2;
            node->children[2]->GCY=(node->LLY+node->GCY)/2;
            // Create the build tree struct
            struct BuildTreeStruct* SW_Data = malloc(sizeof(struct BuildTreeStruct));
            SW_Data->tree = node->children[2];
            SW_Data->sharedBuff = shrdBuff;
            SW_Data->indexes = SWi;
            SW_Data->nShared = SWc;
            SW_Data->remainingThreads = 0;
            // If we have remaining threads to create, we build the sub-tree concurrently
            if (remainingThreads > 0) {
                int assignedThreadsToSubThread = remainingThreadsPerSubThread;

                if (extraThreadsPerSubThread > 0) {
                    assignedThreadsToSubThread++;
                    extraThreadsPerSubThread--;
                }

                remainingThreads = remainingThreads - assignedThreadsToSubThread;

                SW_Data->remainingThreads = assignedThreadsToSubThread;
                pthread_create(&SW_Tid, NULL, (void *(*) (void *)) buildTree, SW_Data);
            } else {
                buildTree(SW_Data);
            }
        } else {
            node->children[2]=NULL;
        }
        if(SEc>0){
            node->children[3]= malloc(sizeof *node->children[3]);
            node->children[3]->TRX=node->TRX;
            node->children[3]->TRY=node->GCY;
            node->children[3]->LLX=node->GCX;
            node->children[3]->LLY=node->LLY;
            node->children[3]->GCX=(node->GCX+node->TRX)/2;
            node->children[3]->GCY=(node->LLY+node->GCY)/2;
            // Create the build tree struct
            struct BuildTreeStruct* SE_Data = malloc(sizeof(struct BuildTreeStruct));
            SE_Data->tree = node->children[3];
            // Copy the shrdBuff to a new buffer, for avoiding multiple accessing issues
            SE_Data->sharedBuff = shrdBuff;
            SE_Data->indexes = SEi;
            SE_Data->nShared = SEc;
            SE_Data->remainingThreads = 0;
            // If we have remaining threads to create, we build the sub-tree concurrently
            if (remainingThreads > 0) {
                int assignedThreadsToSubThread = remainingThreadsPerSubThread;

                if (extraThreadsPerSubThread > 0) {
                    assignedThreadsToSubThread++;
                    extraThreadsPerSubThread--;
                }

                remainingThreads = remainingThreads - assignedThreadsToSubThread;

                SE_Data->remainingThreads = assignedThreadsToSubThread;
                pthread_create(&SE_Tid, NULL, (void *(*) (void *)) buildTree, SE_Data);
            } else {
                buildTree(SE_Data);
            }
        } else {
            node->children[3]=NULL;
        }
        node->mass=0;
        node->CMX=0;
        node->CMY=0;
        // Wait for the 4 node trees creation to finish
        if (NE_Tid != 0) pthread_join(NE_Tid, NULL);
        if (NW_Tid != 0) pthread_join(NW_Tid, NULL);
        if (SW_Tid != 0) pthread_join(SW_Tid, NULL);
        if (SE_Tid != 0) pthread_join(SE_Tid, NULL);
		//Now that we have finished building the 4 trees beneath this node, we calculate the Center of Mass
		//based on the center of mass of the children
        for(i=0;i<4;i++){
            if(node->children[i]!=NULL){
                node->mass+=node->children[i]->mass;
                node->CMX+=node->children[i]->CMX*node->children[i]->mass;
                node->CMY+=node->children[i]->CMY*node->children[i]->mass;
            }
        }
        node->CMX=node->CMX/node->mass;
        node->CMY=node->CMY/node->mass;
		//And tadaaa
    }
}

void calculateForce(struct Node *tree, double *shrdBuff, double *localBuff, int index){
    double distance = sqrt((tree->CMX-shrdBuff[PX(index)])*(tree->CMX-shrdBuff[PX(index)])+
                           (tree->CMY-shrdBuff[PY(index)])*(tree->CMY-shrdBuff[PY(index)]));
	//First we check if the node is not actually the same particle we are calculating
    if(distance>0){
		//Now, we know it is not because the is some distance between the Center of Mass and the particle
		//If the node is external (only contains one particle) or is far away enough, we calculate the force with the center of mass
        if(distance>rcutoff || tree->external){
            double f;
            if(distance<rlimit){
                f=G*tree->mass/(rlimit*rlimit*distance);
            } else {
                f=G*tree->mass/(distance*distance*distance);
            }
            localBuff[AX(index)]+=f*(tree->CMX-shrdBuff[PX(index)]);
            localBuff[AY(index)]+=f*(tree->CMY-shrdBuff[PY(index)]);
        } else {
			//If not, we recursively call the calculateForce() function in the children that are not empty.
            int i;
            for(i=0;i<4;i++){
                if(tree->children[i]!=NULL){
                    calculateForce(tree->children[i],shrdBuff,localBuff,index);
                }
            }
        }
    }
}

void moveParticle(double *shrdBuff, double *localBuff, int index){
    //Unprecise but fast euler method for solving the time differential equation
	double oldX=shrdBuff[PX(index)];
    double oldY=shrdBuff[PY(index)];
    shrdBuff[PX(index)]+=localBuff[VX(index)]*dt+localBuff[AX(index)]*dt*dt*0.5;
    shrdBuff[PY(index)]+=localBuff[VY(index)]*dt+localBuff[AY(index)]*dt*dt*0.5;
    localBuff[VX(index)]=(shrdBuff[PX(index)]-oldX)/dt;
    localBuff[VY(index)]=(shrdBuff[PY(index)]-oldY)/dt;
}

#ifdef D_GLFW_SUPPORT
void drawParticle(double *shrdBuff, double *radius, int index){
    glBegin(GL_TRIANGLE_FAN);
    int k;
    glVertex2f(shrdBuff[PX(index)],shrdBuff[PY(index)]);
    for(k=0;k<20;k++){
        float angle=(float) (k)/19*2*3.141592;
        glVertex2f(shrdBuff[PX(index)]+radius[index]*cos(angle),shrdBuff[PY(index)]+radius[index]*sin(angle));
    }
    glEnd();
}

void drawBarnesHutDivisions(struct Node *rootNode){
    if(!rootNode->external){
        glBegin(GL_LINES);
        glVertex2f(rootNode->GCX,rootNode->LLY);
        glVertex2f(rootNode->GCX,rootNode->TRY);
        glVertex2f(rootNode->LLX,rootNode->GCY);
        glVertex2f(rootNode->TRX,rootNode->GCY);
        glEnd();
        int i;
        for(i=0;i<4;i++){
            if(rootNode->children[i]!=NULL){
                drawBarnesHutDivisions(rootNode->children[i]);
            }
        }
    }
}
#endif

void SaveGalaxy(int count, int nShared, int *indexes, double *sharedBuff);
void SaveGalaxyFile(char *filename, int nShared, int *indexes, double *sharedBuff);

void SaveGalaxy(int count, int nShared, int *indexes, double *sharedBuff)
{
    char filename[100];
    sprintf(filename,"./res/galaxy_%dB_%di.out",nShared,count);
    SaveGalaxyFile(filename, nShared, indexes, sharedBuff);
}

void SaveGalaxyFile(char *filename, int nShared, int *indexes, double *sharedBuff)
{
    int i;
    FILE *res = fopen(filename,"w");

    fprintf(res,"%d\n",nShared);
    for(i=0;i<nShared;i++){
        fprintf(res,"%d\t%e\t%e\t%e\n",indexes[i],sharedBuff[PX(indexes[i])],sharedBuff[PY(indexes[i])],sharedBuff[MASS(indexes[i])]);
    }
    fclose(res);
}

void ReadGalaxyFile(char *filename, int *nShared, int **indexes, double **sharedBuff)
{
    int i,ind;
    FILE *input;

    printf("Reading file %s\n",filename);

    input = fopen(filename,"r");
    if (input==NULL) {
        printf("Error opening file.\n");
        exit(1);
    }
    // Read number of bodies.
    if (fscanf(input,"%d\n",nShared)<1){
        printf("Error reading number of particles.\n");
        exit(1);
    }


    printf("Reading %d bodies\n",*nShared);

    // Reserve memory for indexes and particles.
    *indexes = (int*) malloc(sizeof(int)*(*nShared));
    *sharedBuff = (double *) malloc(sizeof(double)*(3*(*nShared)+1));

    for(i=0;i<(*nShared);i++){
        (*indexes)[i]=i;
    }

    for(i=0;i<(*nShared);i++){
        if (fscanf(input,"%d\t%le\t%le\t%le\n", &ind,&((*sharedBuff)[PX((*indexes)[i])]),&((*sharedBuff)[PY((*indexes)[i])]),&((*sharedBuff)[MASS((*indexes)[i])]))<4){
            printf("Error reading number of particles.\n");
            exit(1);
        }
        //printf("Body %d: (%le,%le) %le\n", ind,(*sharedBuff)[PX((*indexes)[i])],(*sharedBuff)[PY((*indexes)[i])],(*sharedBuff)[MASS((*indexes)[i])]);
    }

    fclose(input);
}

#define DSaveIntermediateState 1
#define DIntervalIntermediateState 100
#define DShowStatistics 1
#define DIntervalStatistics 1

clock_t StartTime, EndTime;
double TimeSpent;

void ShowWritePartialResults(int count,int nOriginal, int nShared, int *indexes, double *sharedBuff)
{
    if (DSaveIntermediateState && !(count % DIntervalIntermediateState))
        SaveGalaxy(count, nOriginal, indexes, sharedBuff);

    if (DShowStatistics && !(count % DIntervalStatistics))
    {
        int i=0;
        double CurrentTime;
        CurrentTime = clock();
        TimeSpent = (double)(CurrentTime - StartTime) / CLOCKS_PER_SEC;
        //Mins = (int)TimeSpent/60;
        //Secs = (TimeSpent-(Mins*60));
        printf("[%.3f] Iteration %d => %d Bodies (%d) \t(Body %d: (%le, %le) %le).\n",TimeSpent, count, nShared, nOriginal, i, sharedBuff[PX(indexes[i])],sharedBuff[PY(indexes[i])],sharedBuff[MASS(indexes[i])]);
    }
}

int GraphicInterface(struct GraphicInterfaceStruct *data) {

    //Unpack the necessarly variables from the data struct
    struct Node* tree = data->tree;
    double* sharedBuff = data->sharedBuff;
    int* indexes = data->indexes;
    int nShared = data->nShared;
    int nLocal = data->nLocal;
    double* localBuff = data->localBuff;
    double* radius = data->radius;
    int count = data->count;
    int steps = data->steps;
    int nOriginal = data->nOriginal;
    int M = data->M;

    //If you only care about the algorithm, skip until next comment
    if(!glfwInit()){
        printf("Failed to start GLFW\n");
        return -1;
    }
    GLFWwindow *window = glfwCreateWindow(2000,2000,"Simulation",NULL,NULL);
    if(!window){
        printf("Failed to open window\n");
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0,1,0,1,0,1);
    glMatrixMode(GL_MODELVIEW);

    while(!glfwWindowShouldClose(window) && count<=steps){
        glClear(GL_COLOR_BUFFER_BIT);

        double t=glfwGetTime();

        //We build the tree, which needs a pointer to the initial node, the buffer holding position and mass of the particles, indexes and number of particles
        // Create a BuildTreeStruct for passing the params to the buildTree function
        struct BuildTreeStruct* data = malloc(sizeof(struct BuildTreeStruct));
        data->tree = tree;
        data->sharedBuff = sharedBuff;
        data->indexes = indexes;
        data->nShared = nShared;
        data->remainingThreads = M;

        buildTree(data);

        //Now that it is built, we calculate the forces per particle
        for(int i=0;i<nLocal;i++){
            //First we make them zero in both directions
            localBuff[AX(indexes[i])]=0;
            localBuff[AY(indexes[i])]=0;
            int s;
            for(s=0;s<4;s++){
                //Now, for each children that is not empty, we calculate the force (the calculateForce() function is recursive)
                if(tree->children[s]!=NULL)
                    calculateForce(tree->children[s],sharedBuff,localBuff,indexes[i]);//       ¡¡¡ HOT POINT !!!
            }
            //We calculate the new position of the particles according to the accelerations
            moveParticle(sharedBuff,localBuff,indexes[i]);
            //This is to kick out particles that escape the rectangle (0,1)x(0,1), so we just delete the index.
            if(sharedBuff[PX(indexes[i])]<=0 || sharedBuff[PX(indexes[i])]>=1 || sharedBuff[PY(indexes[i])] <=0 || sharedBuff[PY(indexes[i])] >= 1){
                int r;
                nLocal--;
                nShared--;
                for(r=i;r<nLocal;r++){
                    indexes[r]=indexes[r+1];
                }
                i--;
            }
        }

        SaveGalaxy(count, nShared, indexes, sharedBuff);

        //This is only for visualization
        drawBarnesHutDivisions(tree);
        int k;
        for(k=0;k<nShared;k++){
            drawParticle(sharedBuff,radius,indexes[k]);
        }

        t=glfwGetTime()-t;
        if(t<0.013){
            usleep(1000*1000*(0.013-t));
        }

        glfwSwapBuffers(window);
        glfwPollEvents();

        ShowWritePartialResults(count, nOriginal, nShared, indexes, sharedBuff);

        //We advance one step
        count++;
    }
    glfwTerminate();
    return 0;
}

int main(int argc, char *argv[]){
    int nShared=500;
	int steps=100;
    int M=4;
    double *sharedBuff;
    double *localBuff;
    double *radius;
    int *indexes, i;
    char filename[100];

    printf("NBody with %d arguments.\n",argc);
    StartTime = clock();


	if(argc>1){
		nShared=atoi(argv[1]);
		if(argc>2){
			steps=atoi(argv[2]);
            if(argc>4){
                M=atoi(argv[4]);
            }
		}
	}

    if(argc>3 && access(argv[3], F_OK) == 0)
    {
        printf("Read file..\n");
        /* Read bodies initial state from file */
        ReadGalaxyFile(argv[3], &nShared, &indexes, &sharedBuff);
        argc--;
    }
    else
    {   /* Inicialize the bodies randomly */

        //Buffers to hold the position of the particles and their mass
        sharedBuff = (double *) malloc(sizeof(double) * (3 * nShared + 1));

        srand(time(NULL));
        for (i = 0; i < nShared; i++) {
            //I start with an almost random distribution of particles
            sharedBuff[PX(i)] = (float) (i) / (nShared - 1) * 0.8 + 0.1;
            sharedBuff[PY(i)] = (float) (rand() % 4096) / 4095 * 0.8 + 0.1;
            //With a random Mass between 1 and 3
            sharedBuff[MASS(i)]=(double) (rand()%2048)/2047*2+1;
        }

        //Index array, to speed up the creation of the tree (faster than passing the 3 floats per particle of x,y and mass)
        indexes = (int*) malloc(sizeof(int)*nShared);
        for(i=0;i<nShared;i++){
            indexes[i]=i;
        }
    }

    int nLocal=nShared;
    int nOriginal=nShared;
    //Buffer to hold velocity in x and y, and acceleration in x and y also
    localBuff = (double *) malloc(sizeof(double)*(4*nLocal));
    //This is for opengl
    radius = (double *) malloc(sizeof(double)*(nShared));

    for(i=0;i<nShared;i++){
        // init bodies mass
        radius[i]=sqrt(sharedBuff[MASS(i)])*0.0025;

        //With zero speed, and zero acceleration
        localBuff[VX(i)]=0;
        localBuff[VY(i)]=0;
        localBuff[AX(i)]=0;
        localBuff[AY(i)]=0;
    }

    //This is the main node, the one that holds the first four children nodes that make the calculation zone
    struct Node* tree = malloc(sizeof *tree);
	//LLX is the x coordinate of the Low Left corner
    tree->LLX=0;
	//This is the y coordinate..
    tree->LLY=0;

	//Now the same but for the top right corner
    tree->TRX=1;
    tree->TRY=1;
	//The coordinates of the geometric center of the node in x and y
    tree->GCX=0.5;
    tree->GCY=0.5;

    // Save initial state.
    sprintf(filename,"./res/galaxy_%dB_initial.out",nOriginal);
    SaveGalaxyFile(filename, nShared, indexes, sharedBuff);

    int count=1;
	//If we need to visualize
#ifdef D_GLFW_SUPPORT
	if(argc>3){
        // Create struct for passing through argument all the necessary data for GraphicInterface function
        struct GraphicInterfaceStruct* data = malloc(sizeof (struct GraphicInterfaceStruct));
        data->tree = tree;
        data->sharedBuff = sharedBuff;
        data->indexes = indexes;
        data->nShared = nShared;
        data->nLocal = nLocal;
        data->localBuff = localBuff;
        data->radius = radius;
        data->count = count;
        data->steps = steps;
        data->nOriginal = nOriginal;
        data->M = M;

        // Execute the graphic version in another thread
        pthread_t graphicTid;
        pthread_create(&graphicTid, NULL, (void *(*) (void *)) GraphicInterface, data);

        // Wait for it to finish
        pthread_join(graphicTid, NULL);
	} else {
#endif
		//This is the pure algorithm, without visualization
		//system("mkdir res");
    	while(count<=steps){
			//First we build the tree
            // Create a BuildTreeStruct for passing the params to the buildTree function
            struct BuildTreeStruct* data = malloc(sizeof(struct BuildTreeStruct));
            data->tree = tree;
            data->sharedBuff = sharedBuff;
            data->indexes = indexes;
            data->nShared = nShared;
            data->remainingThreads = M;
        	buildTree(data);

        	for(i=0;i<nLocal;i++){
				//Set initial accelerations to zero
            	localBuff[AX(indexes[i])]=0;
            	localBuff[AY(indexes[i])]=0;
            	int s;
            	for(s=0;s<4;s++){
					//Recursively calculate accelerations
                	if(tree->children[s]!=NULL)
                		calculateForce(tree->children[s],sharedBuff,localBuff,indexes[i]);
            	}
				//Calculate new position
            	moveParticle(sharedBuff,localBuff,indexes[i]);
            	//Kick out particle if it went out of the box (0,1)x(0,1)
				if(sharedBuff[PX(indexes[i])]<=0 || sharedBuff[PX(indexes[i])]>=1 || sharedBuff[PY(indexes[i])] <=0 || sharedBuff[PY(indexes[i])] >= 1){
                	int r;
                	nLocal--;
                	nShared--;
                	for(r=i;r<nLocal;r++){
                    	indexes[r]=indexes[r+1];
                	}
                	i--;
            	}
        	}
			
			//To be able to store the positions of the particles
            ShowWritePartialResults(count,nOriginal, nShared, indexes, sharedBuff);
            //We advance one step
			count++;
		}
#ifdef D_GLFW_SUPPORT
	}
#endif

    EndTime = clock();
    TimeSpent = (double)(EndTime - StartTime) / CLOCKS_PER_SEC;
    printf("NBody Simulation took %.3f seconds.\n",TimeSpent);

    // Save initial state.
    sprintf(filename,"./res/galaxy_%dB_%di_final.out",nOriginal, count-1);
    SaveGalaxyFile(filename, nShared, indexes, sharedBuff);

	free(sharedBuff);
	free(localBuff);
	free(radius);
	free(indexes);

    return 0;
}
