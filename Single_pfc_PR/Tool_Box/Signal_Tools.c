/*
 * Signal_Tools.c
 *
 *  Created on: Jul 9, 2025
 *      Author: HuaWenxi
 */
#include"Signal_Tools.h"
//如果会用stm32的fft库，使用那个fft库函数应该是最稳定的方法
//-----------------------------	FFT-华文熙编写-------------------------
void fastFourierOperation()
{
	int l=0;
	complexNumber up,down,product;
	changePosition();                  //偶奇变换算法
	for(int i=0;i<size_x/2;i++)        //一级蝶形运算
	{
		l=1<<i;
		for(int j=0;j<size_x;j+= 2*l ) //一组蝶形运算
		{
			for(int k=0;k<l;k++)       //一个蝶形运算
			{
				mul(x[j+k+l],W[size_x*k/2/l],&product);
				add(x[j+k],product,&up);
				sub(x[j+k],product,&down);
				x[j+k]=up;
				x[j+k+l]=down;
			}
		}
	}
}


/*生成旋转因子数组*/
void initRotationFactor()
{
	for(int i=0;i<size_x/2;i++)
	{
		W[i].realPart=cos(2*PI/size_x*i);//用欧拉公式计算旋转因子
		W[i].imaginaryPart=-1*sin(2*PI/size_x*i);
	}
}


/*偶奇变换算法*/
void changePosition()
{
	int j=0,k;
	complexNumber temp;

	for (int i=0;i<size_x-1;i++)
	{
		if(i<j)
		{
			//若倒序数大于顺序数，进行变址（换位）；否则不变，防止重复交换，变回原数
			temp=x[i];
			x[i]=x[j];
			x[j]=temp;
		}
		k=size_x/2;    //判断j的最高位是否为0
		while(j>=k)
		{              //最高位为1
			j=j-k;
			k=k/2;
		}
		j=j+k;        //最高位为0
	}
}

/*复数加法的定义*/
void add(complexNumber a,complexNumber b,complexNumber *c)
{
	c->realPart=a.realPart+b.realPart;
	c->imaginaryPart=a.imaginaryPart+b.imaginaryPart;
}

/*复数乘法的定义*/
void mul(complexNumber a,complexNumber b,complexNumber *c)
{
	c->realPart=a.realPart*b.realPart - a.imaginaryPart*b.imaginaryPart;
	c->imaginaryPart=a.realPart*b.imaginaryPart + a.imaginaryPart*b.realPart;
}

/*复数减法的定义*/
void sub(complexNumber a,complexNumber b,complexNumber *c)
{
	c->realPart=a.realPart-b.realPart;
	c->imaginaryPart=a.imaginaryPart-b.imaginaryPart;
}
//----------------------------FFT Yuyang学长编写---------------------
// ������
complex c_plus(complex a, complex b)
{
    complex c;
    c.real = a.real + b.real;
    c.imag = a.imag + b.imag;
    return c;

}

// ������
complex c_sub(complex a, complex b)
{
    complex c;
    c.real = a.real - b.real;
    c.imag = a.imag - b.imag;
    return c;

}

// ������
complex c_mul(complex a, complex b)
{
    complex c;
    c.real = a.real * b.real - a.imag * b.imag;
    c.imag = a.real * b.imag + a.imag * b.real;
    return c;

}

// ������
complex c_div(complex a, complex b)
{
    complex c;
    c.real = (a.real * b.real + a.imag * b.imag)/(b.real * b.real +b.imag * b.imag);
    c.imag = (a.imag * b.real - a.real * b.imag)/(b.real * b.real +b.imag * b.imag);
    return c;

}

// ��������
complex c_conjugate(complex a)
{
    complex b;
    b.real = a.real;
    b.imag = -a.imag;
    return b;
}

// ������ģ
float c_abs(complex a)
{
	float b;
	arm_sqrt_f32(a.real*a.real + a.imag*a.imag,&b);
    return b;

}

// ���������ģ
void cArray_abs(int size, complex *a, float *abs)
{
    int i;
    for(i = 0; i < size; i++)
    {
        abs[i] = c_abs(a[i]);
    }

}

// λ��ת�û�
void trans(int size_x, complex *x)
{
    int p = 0; int a, b;
    int i;
    for (i = 1; i < size_x; i *= 2)
    {
        p++;    // ���������λ��
    }
    for (i = 0; i < size_x; i++)
    {
        a = i;
        b = 0;
        int j;
        for (j = 0; j < p; j++)
        {
            b = (b << 1) + (a & 1);     // b�洢��ǰ�±�Ļ���ֵ
            a = a >> 1;
        }
        if (b > i)  // �����ظ�����
        {
            complex temp;
            temp = x[i];
            x[i] = x[b];
            x[b] = temp;
        }
    }
}

// ���Wn��ֵ
complex getWn_i(int size, int i)
{
    complex Wn_i;
    double real = arm_cos_f32(-2 * PI * i / size);
    double img = arm_sin_f32(-2 * PI * i / size);
    Wn_i.real = real,
    Wn_i.imag = img;

    return Wn_i;

}

// ���ٸ���Ҷ�任FFT
void fft(int size, complex *x, complex *X)
{
    int i;
    for (i = 0; i < size; i++)
    {
        X[i] = x[i];
    }
    trans(size, X);  // λ��ת�û�
    int t;
    int m;
    for (m = 2; m <= size; m *= 2)  // С���е���
    {
        int k;
        for (k = 0; k < size; k += m)   // С������ʼ�±�
        {
            int j;
            for (j = 0; j < m / 2; j++)     // С���е�DFT����
            {
                int index1 = k + j;
                int index2 = index1 + m / 2;
                t = j * size / m;               // t�������������е��±꣬�ҵ���Ӧ����ת����
                complex temp1,temp2;
                complex Wn_i;
                Wn_i = getWn_i(size, t);
                temp2 = c_mul(X[index2], Wn_i);
                temp1 = X[index1];
                X[index1] = c_plus(temp1, temp2);  // Wn������
                X[index2] = c_sub(temp1, temp2);
            }
        }
    }

}

// ����Ҷ��任
void ifft(int size, complex *X, complex *x)
{
    int i;
    for (i = 0; i < size; i++)
    {
        x[i] = c_conjugate(X[i]);
    }
    fft(size, x, x);
    for (i = 0; i < size; i++)
    {
        x[i].imag = (x[i].imag) / size;
        x[i].real = (x[i].real) / size;
    }

}



