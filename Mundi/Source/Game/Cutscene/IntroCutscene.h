	bool IsWidgetAnimationDone(const std::string& WidgetName);

	/**
	 * @brief 멤버 위젯들(mb1~4)의 애니메이션이 모두 완료되었는지 확인
	 */
	bool AreMembersAnimationDone();

	/**
	 * @brief 리소스 정리
	 */
	void Cleanup();
};
