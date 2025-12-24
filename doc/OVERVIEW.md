This is an overview of the source code organisation: 

```
.
├── cmake/deps
├── CMakeLists.txt
├── compute
├── core
├── doc
├── example
├── LICENSE
└── README.md
``` 

- `cmake/dep` folder allow us to package dependencies so there is no install to do to have the header files we use. The header files we use are: 
  - headers from nvidia to communicate with the GPU. 
  - headers from Vulkan to allow us to do the interop from our computed images we compute to a displayed image. 
  - headers from GLFW to handle user inputs. 
- `compute` is the CUDA library that handle all the GPU operations of our engine.
- `core` is the library that handle all the operations that our GPU do not (e.g. interop with Vulkan...). 
- `doc` is our documnentation directory, you will find here all the source file of the complete project documentation. The light version of the documentation can be found in the [wiki](https://github.com/BuzzYbis/gamegine/wiki) section of this repository. 
- `example` is a demonstration of a usage of our engine so can compile it and see what it does.
