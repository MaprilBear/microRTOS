/**
 * @file      Interpreter.h
 * @brief     Real Time Operating System for Labs 2, 3 and 4
 * @details   EE445M/EE380L.6
 * @version   V1.0
 * @author    Valvano
 * @copyright Copyright 2020 by Jonathan W. Valvano, valvano@mail.utexas.edu,
 * @warning   AS-IS
 * @note      For more information see  http://users.ece.utexas.edu/~valvano/
 * @date      Jan 5, 2020

 ******************************************************************************/


#include <stdint.h>
#include "../RTOS_Labs_common/OS.h"


/**
 * @details  Run the user interface.
 * @param  none
 * @return none
 * @brief  Interpreter
 */
void Interpreter(bool useLCD, bool esp, void(*task)(void), Sema4Type* sema);

