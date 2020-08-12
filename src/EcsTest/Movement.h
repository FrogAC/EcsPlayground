enum class MoveState : char {
    //  01   | 01 
    //  Lbits| Rbits
    None = 0,
    Right1 = 0b0001,
    Right2 = 0b0010, 
    Right3 = 0b0011,
    Left1  = 0b0100,
    Left2  = 0b1000,
    Left3  = 0b1100
};