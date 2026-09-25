#[derive(Clone, Copy)]
struct Meter {
    value: i32,
}

impl Meter {
    const ZERO: i32 = 0i32;

    fn new(value: i32) -> Self {
        Self { value: value }
    }

    fn get(&self) -> i32 {
        self.value
    }

    fn add(&mut self, amount: i32) {
        self.value += amount;
    }

    fn reset(mut self) -> Self {
        self.value = Self::ZERO;
        self
    }

    fn into_value(self) -> i32 {
        self.value
    }
}
