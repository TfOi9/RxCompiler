struct Point {
    x: i32,
    y: i32,
}

impl Point {
    fn new(x: i32, y: i32) -> Self {
        Self { x: x, y: y }
    }

    fn translate(&mut self, dx: i32, dy: i32) {
        self.x += dx;
        self.y += dy;
    }

    fn sum(&self) -> i32 {
        self.x + self.y
    }
}

fn project(points: [Point; 2], index: usize) -> i32 {
    let mut point = Point::new(points[index].x, points[index].y);
    point.translate(1i32, 2i32);
    let sum = (point.sum() as i32);
    let copy = Point { x: sum, y: point.y };
    copy.x + copy.y
}
