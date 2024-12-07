
#import <TargetConditionals.h>

#if TARGET_OS_IOS
#import <UIKit/UIKit.h>

UITextField *inputTextField;

void ShowKeyboard() {
    if (!inputTextField) {
        inputTextField = [[UITextField alloc] initWithFrame:CGRectMake(0, 0, 1, 1)];
        inputTextField.hidden = YES; // Hide the field visually.
        [[UIApplication sharedApplication].keyWindow addSubview:inputTextField];
    }
    [inputTextField becomeFirstResponder];
}

void HideKeyboard() {
    if (inputTextField) {
        [inputTextField resignFirstResponder];
    }
}
